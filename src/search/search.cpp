#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "core/constants.hpp"
#include "eval/evaluator.hpp"
#include "search/move_picker.hpp"
#include "search/search_worker.hpp"
#include "search/tt.hpp"

namespace {

// Aspiration-window defaults.
constexpr EvalValue AspirationWindow = 50;

constexpr std::array<int, 20> HelperDepthSkipSize{
    1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
};
constexpr std::array<int, 20> HelperDepthSkipPhase{
    0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7,
};

// Null-move pruning defaults.
constexpr int NullMoveReductionBase = 3;
constexpr int NullMoveDeepReduction = 4;
constexpr int NullMoveDeepThreshold = 6;

// Razoring and futility defaults.
constexpr int RazorMaxDepth    = 3;
constexpr int FutilityMaxDepth = 3;
constexpr int RazorMargin[]    = {0, 500, 900, 1800};
constexpr int FutilityMargin[] = {0, 250, 400, 550};

// Late-move reduction defaults.
constexpr int LmrMinDepth     = 3;
constexpr int LmrMinMoveCount = 4;

// Quiet-history malus defaults.
constexpr int QuietMalusMinDepth  = 4;
constexpr int QuietMalusMinFailed = 2;
constexpr int QuietMalusDivisor   = 2;

// Apply the PV/non-PV TT cutoff policy.
template <NodeType Node>
bool tt_cutoff_allowed(
    const TTRecord& record, EvalValue adjusted_score, int depth, EvalValue alpha, EvalValue beta) {
    if constexpr (Node == NodeType::Pv)
        return int(record.depth) >= depth && record.bound == TTBound::Exact;

    return record.can_cutoff(adjusted_score, depth, alpha, beta);
}

// Late-move reduction formula.
template <NodeType Node>
int lmr_reduction(int  depth,
                  int  move_count,
                  bool is_quiet,
                  bool is_promotion,
                  bool in_check,
                  bool gives_check,
                  bool is_killer) {
    if (depth < LmrMinDepth || move_count < LmrMinMoveCount)
        return 0;

    // Do not reduce moves that create immediate tactical obligations.
    if (is_promotion || in_check || gives_check)
        return 0;

    // Use the LMR formula as a starting point.
    const double base = is_quiet ? 1.25 : 0.75;
    const double div  = is_quiet ? 2.5 : 3.3;
    double       r    = base + std::log(depth) * std::log(move_count) / div;

    // Reduce less for PV and killer moves.
    if constexpr (Node == NodeType::Pv)
        r *= 0.7;
    if (is_killer)
        r *= 0.8;

    // Do not extend or drop straight into qsearch.
    return std::clamp(static_cast<int>(r), 1, depth - 2);
}

struct FailedQuiets {
    static constexpr int Capacity = 32;

    bool add(Move move) {
        if (count_ >= Capacity)
            return false;

        moves_[count_++] = move;
        return true;
    }

    int size() const { return count_; }

    template <typename Fn>
    void for_each(Fn fn) const {
        for (int i = 0; i < count_; ++i)
            fn(moves_[i]);
    }

private:
    Move moves_[Capacity];
    int  count_{0};
};

} // namespace

// Main root search driver.
EvalValue SearchWorker::search_root() {
    // Terminal root: return immediately when no legal root move exists.
    if (root_lines.empty()) {
        root_result = terminal_root_result();
        return root_result.value;
    }

    // Iterative deepening searches one completed depth at a time.
    for (int depth = 1; depth <= limits.depth && !stop_requested(); ++depth) {
        if (!should_search_root_depth(depth))
            continue;
        if (!search_root_depth(depth, root_result.value))
            break;
    }

    return root_result.value;
}

bool SearchWorker::should_search_root_depth(int depth) const noexcept {
    if (is_main_worker() || depth == 1)
        return true;

    // Stagger helper depths to diversify shared-TT work.
    const size_t index = static_cast<size_t>(worker_id - 1) % HelperDepthSkipSize.size();
    return ((depth + HelperDepthSkipPhase[index]) / HelperDepthSkipSize[index]) % 2 == 0;
}

// Root aspiration loop for a single depth.
bool SearchWorker::search_root_depth(int depth, EvalValue previous_value) {
    EvalValue delta = AspirationWindow;
    EvalValue alpha = std::max(previous_value - delta, -eval_value::inf);
    EvalValue beta  = std::min(previous_value + delta, eval_value::inf);

    while (!stop_requested()) {
        // Keep root order but clear stale attempt state.
        for (RootLine& line : root_lines) {
            line.reset_attempt();
        }

        // Search this depth inside the current aspiration window.
        if (!search_root_window(depth, alpha, beta))
            return false;

        // Promote the best completed root line.
        std::stable_sort(root_lines.begin(), root_lines.end(), is_better_root_line);
        const RootLine& best_line = root_lines.front();
        assert(best_line.has_completed_depth());
        const EvalValue value = best_line.value;
        assert(value > -eval_value::inf && value < eval_value::inf);

        if (value <= alpha) {
            // Fail low: widen the lower bound and re-search.
            stats.aspiration_fail_low();
            alpha = std::max(alpha - delta, -eval_value::inf);
        } else if (value >= beta) {
            // Fail high: widen the upper bound and re-search.
            stats.aspiration_fail_high();
            beta = std::min(beta + delta, eval_value::inf);
        } else {
            // Window hit: accept and publish the completed depth.
            root_result = best_line;
            publish_root_snapshot();
            if (is_main_worker())
                report_root_progress(root_result);
            return true;
        }

        // Increase retry width after each aspiration miss.
        delta = delta >= eval_value::inf / 2 ? eval_value::inf : delta * 2;
    }

    return false;
}

// Fixed-window root pass. Caller owns attempt reset and result ordering.
bool SearchWorker::search_root_window(int depth, EvalValue alpha, EvalValue beta) {
    assert(!root_lines.empty());

    int  move_count  = 0;
    bool has_pv_move = false;

    // Preserve caller order for iterative deepening and aspiration retries.
    for (RootLine& line : root_lines) {
        assert(line.has_root_move());
        const Move root_move = line.root_move;
        assert(board.is_legal_pseudo_move(root_move));

        ++move_count;

        board.make(root_move);
        ++search_ply;

        // Root PVS searches full-window until a root PV is established.
        // Scout later root moves and re-search only strict alpha improvements.
        PrincipalVariation child_pv;
        EvalValue          value;
        if (move_count == 1 || !has_pv_move) {
            value = -alphabeta<NodeType::Pv>(-beta, -alpha, depth - 1, &child_pv);
        } else {
            value = -alphabeta<NodeType::NonPv>(-alpha - 1, -alpha, depth - 1);
            if (!stop_requested() && value > alpha) {
                stats.pvs_research(search_ply);
                child_pv.clear();
                value = -alphabeta<NodeType::Pv>(-beta, -alpha, depth - 1, &child_pv);
            }
        }

        board.unmake();
        --search_ply;

        // Do not record partial root-line state after a stop.
        if (stop_requested())
            return false;

        line.complete(depth, value, child_pv);

        // Let aspiration handle the fail-high window miss.
        if (value >= beta)
            return true;

        if (value > alpha) {
            // A new root move improved alpha.
            if (is_main_worker())
                report_root_progress(line);

            alpha       = value;
            has_pv_move = true;
        } else if (move_count > 1 && has_pv_move && value == alpha && alpha > -eval_value::inf) {
            // Keep the full-window alpha raiser ordered first on scout ties.
            line.value = alpha - 1;
        }
    }

    return true;
}

// Recursive main search: alpha-beta for non-PV nodes, PVS for PV nodes.
template <NodeType Node>
EvalValue SearchWorker::alphabeta(
    EvalValue alpha, EvalValue beta, int depth, PrincipalVariation* pv, bool can_null) {
    // Step 1. PV and stop checks.
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    // Step 2. Draw, max-ply, and qsearch exits.
    const bool drawn = board.is_draw(search_ply);

    if (drawn) {
        increment_nodes();
        stats.node(search_ply);
        return eval_value::draw;
    }

    if (search_ply >= engine::max_search_ply) {
        increment_nodes();
        stats.node(search_ply);
        return evaluate(board);
    }

    if (depth <= 0)
        return quiescence<Node>(alpha, beta, pv);

    increment_nodes();
    stats.node(search_ply);

    // Step 3. Mate-distance pruning.
    alpha = std::max(alpha, -eval_value::mate + search_ply);
    beta  = std::min(beta, eval_value::mate - search_ply - 1);
    if (alpha >= beta)
        return alpha;

    const EvalValue   original_alpha = alpha;
    const PositionKey position_key   = board.key();
    Move              tt_move        = NULL_MOVE;

    // Step 4. TT probe.
    stats.main_tt_probe(search_ply);
    const auto tt_record = tt.probe(position_key);
    if (tt_record) {
        stats.main_tt_hit(search_ply);

        const TTRecord& record   = *tt_record;
        const EvalValue tt_score = record.score_at_ply(search_ply);
        if (tt_cutoff_allowed<Node>(record, tt_score, depth, alpha, beta)) {
            stats.main_tt_cutoff(search_ply);
            return tt_score;
        }

        tt_move = record.move;
    }

    const bool  in_check = board.is_check();
    const Color side     = board.side_to_move();
    bool        futility = false;

    if constexpr (Node == NodeType::NonPv) {
        // Step 5. Razoring.
        const EvalValue static_eval = evaluate(board);
        if (can_null && !in_check && depth <= RazorMaxDepth && tt_move.is_null()
            && static_eval + RazorMargin[depth] <= alpha) {
            stats.razor_try(search_ply);
            const EvalValue value = quiescence<NodeType::NonPv>(alpha - 1, alpha);
            if (stop_requested())
                return alpha;
            if (value < alpha) {
                stats.razor_cutoff(search_ply);
                return value;
            }
        }

        // Step 6. Null-move pruning.
        // Skip NMP when a depth-sufficient TT upper bound suggests it will fail low.
        const int reduction =
            depth > NullMoveDeepThreshold ? NullMoveDeepReduction : NullMoveReductionBase;
        const bool tt_upper_veto = tt_record && tt_record->depth >= depth
                                && tt_record->bound == TTBound::UpperBound
                                && tt_record->score_at_ply(search_ply) < beta;
        if (can_null && !in_check && depth >= reduction
            && board.non_pawn_material(side) > piece_value::rook_mg && !tt_upper_veto) {
            stats.null_move_try(search_ply);

            board.make_null();
            ++search_ply;
            const EvalValue value =
                -alphabeta<NodeType::NonPv>(-beta, -beta + 1, depth - reduction, nullptr, false);
            board.unmake_null();
            --search_ply;

            if (stop_requested())
                return alpha;
            if (value >= beta) {
                stats.null_move_cutoff(search_ply);
                return value;
            }
        }

        // Prepare shallow futility pruning. The move loop performs the actual skip.
        futility = depth <= FutilityMaxDepth && !in_check && alpha > -eval_value::mate_bound
                && alpha < eval_value::mate_bound && static_eval + FutilityMargin[depth] <= alpha;
    }

    // Step 7. Move ordering and quiet-malus tracking.
    int       move_count = 0;
    EvalValue best_value = -eval_value::inf;
    Move      best_move  = NULL_MOVE;

    const auto context = MoveOrdering::make_context(board);
    auto       picker  = move_picker::main_search(board, ordering, context, search_ply, tt_move);

    PrincipalVariation child_pv;
    FailedQuiets       failed_quiets;

    const bool allow_quiet_malus = depth >= QuietMalusMinDepth && !in_check;
    if (allow_quiet_malus)
        stats.quiet_malus_eligible_node(depth);

    // Step 8. Move loop.
    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (!board.is_legal_pseudo_move(move))
            continue;

        ++move_count;
        const bool first_legal = move_count == 1;

        const bool is_promotion = move.type() == MOVE_PROM;
        const bool is_capture   = board.is_capture(move);
        const bool is_quiet     = !is_capture && !is_promotion;
        const bool is_killer    = is_quiet && ordering.is_killer(move, search_ply);
        board.make(move);
        ++search_ply;

        const bool gives_check = board.is_check();
        if (futility && !first_legal && is_quiet && !gives_check) {
            // Step 9. Futility pruning.
            board.unmake();
            --search_ply;
            picker.skip_quiet_moves();
            stats.futility_skip(search_ply);
            continue;
        }

        // Step 10. Late-move reductions.
        // If the reduced search beats alpha, research the move at full depth.
        EvalValue value;
        const int reduction = lmr_reduction<Node>(
            depth, move_count, is_quiet, is_promotion, in_check, gives_check, is_killer);
        if (reduction > 0) {
            stats.lmr_try(search_ply - 1);
            value = -alphabeta<NodeType::NonPv>(
                -alpha - 1, -alpha, depth - 1 - reduction, nullptr, true);
            if (!stop_requested() && value > alpha) {
                stats.lmr_research(search_ply - 1);
                if constexpr (Node == NodeType::Pv) {
                    stats.pvs_research(search_ply);
                    child_pv.clear();
                    value = -alphabeta<NodeType::Pv>(
                        -beta, -alpha, depth - 1, pv ? &child_pv : nullptr, true);
                } else {
                    value = -alphabeta<NodeType::NonPv>(-beta, -alpha, depth - 1, nullptr, true);
                }
            }
        } else {
            // Step 11. Principal variation search.
            if constexpr (Node == NodeType::NonPv) {
                value = -alphabeta<NodeType::NonPv>(-beta, -alpha, depth - 1, nullptr, true);
            } else if (move_count == 1) {
                value = -alphabeta<NodeType::Pv>(
                    -beta, -alpha, depth - 1, pv ? &child_pv : nullptr, true);
            } else {
                value = -alphabeta<NodeType::NonPv>(-alpha - 1, -alpha, depth - 1, nullptr, true);
                if (!stop_requested() && value > alpha) {
                    stats.pvs_research(search_ply);
                    child_pv.clear();
                    value = -alphabeta<NodeType::Pv>(
                        -beta, -alpha, depth - 1, pv ? &child_pv : nullptr, true);
                }
            }
        }

        board.unmake();
        --search_ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Step 12. Beta cutoff.
            if (is_quiet) {
                stats.quiet_cutoff(depth);
                ordering.update_quiet_refutations(context, move, search_ply);
                ordering.reward_quiet(context, board, move, depth);
                if (allow_quiet_malus && failed_quiets.size() >= QuietMalusMinFailed) {
                    failed_quiets.for_each([&](Move quiet) {
                        ordering.penalize_quiet(context, board, quiet, depth, QuietMalusDivisor);
                        stats.quiet_malus_update(depth);
                    });
                }
            }

            if (pv)
                pv->update(move, child_pv);

            stats.beta_cutoff(search_ply, move_count);
            tt.store(position_key, move, value, depth, TTBound::LowerBound, search_ply);
            return value;
        }

        if (allow_quiet_malus && is_quiet && !(move == tt_move) && !is_killer) {
            if (failed_quiets.add(move))
                stats.quiet_malus_failed_quiet(depth);
        }
        // Step 13. Best-move update.
        if (value > best_value) {
            best_value = value;
            best_move  = move;

            if (value > alpha) {
                alpha = value;
                if (pv)
                    pv->update(move, child_pv);
            }
        }
    }

    // Step 14. Mate and stalemate.
    if (move_count == 0) {
        best_value = in_check ? -eval_value::mate + search_ply : eval_value::draw;
        tt.store(position_key, NULL_MOVE, best_value, depth, TTBound::Exact, search_ply);
        return best_value;
    }

    // Step 15. TT store.
    tt.store(position_key,
             best_move,
             best_value,
             depth,
             tt_bound_for_window(best_value, original_alpha, beta),
             search_ply);

    return best_value;
}

// Quiescence search for tactical depth-zero nodes.
template <NodeType Node>
EvalValue SearchWorker::quiescence(EvalValue alpha, EvalValue beta, PrincipalVariation* pv) {
    // Step 1. PV and stop checks.
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    increment_nodes();
    stats.qnode(search_ply);

    // Step 2. Draw and max-ply exits.
    if (board.is_draw(search_ply))
        return eval_value::draw;

    if (search_ply >= engine::max_search_ply)
        return evaluate(board);

    constexpr int     qsearch_tt_depth = 0;
    const EvalValue   original_alpha   = alpha;
    const PositionKey position_key     = board.key();
    Move              tt_move          = NULL_MOVE;

    // Step 3. TT probe.
    stats.q_tt_probe(search_ply);
    if (auto record = tt.probe(position_key)) {
        stats.q_tt_hit(search_ply);

        const EvalValue tt_score = record->score_at_ply(search_ply);
        if (tt_cutoff_allowed<Node>(*record, tt_score, qsearch_tt_depth, alpha, beta)) {
            stats.q_tt_cutoff(search_ply);
            return tt_score;
        }

        tt_move = record->move;
    }

    const bool in_check   = board.is_check();
    int        move_count = 0;
    EvalValue  best_value = -eval_value::inf;
    Move       best_move  = NULL_MOVE;

    // Step 4. Stand pat.
    if (!in_check) {
        best_value = evaluate(board);
        if (best_value >= beta) {
            tt.store(position_key,
                     NULL_MOVE,
                     best_value,
                     qsearch_tt_depth,
                     TTBound::LowerBound,
                     search_ply);
            return best_value;
        }
        if (best_value > alpha)
            alpha = best_value;
    }

    auto               picker = move_picker::qsearch(board, ordering, tt_move);
    PrincipalVariation child_pv;

    // Step 5. Tactical move or evasion loop.
    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (!board.is_legal_pseudo_move(move))
            continue;

        ++move_count;

        board.make(move);
        ++search_ply;
        const EvalValue value = -quiescence<Node>(-beta, -alpha, pv ? &child_pv : nullptr);
        board.unmake();
        --search_ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Step 6. Beta cutoff.
            if (pv)
                pv->update(move, child_pv);
            stats.beta_cutoff(search_ply, move_count);
            tt.store(position_key, move, value, qsearch_tt_depth, TTBound::LowerBound, search_ply);
            return value;
        }

        // Step 7. Best-move update.
        if (value > best_value) {
            best_value = value;
            best_move  = move;
            if (value > alpha) {
                alpha = value;
                if (pv)
                    pv->update(move, child_pv);
            }
        }
    }

    // Step 8. Checkmate.
    if (in_check && move_count == 0) {
        best_value = -eval_value::mate + search_ply;
        tt.store(position_key, NULL_MOVE, best_value, qsearch_tt_depth, TTBound::Exact, search_ply);
        return best_value;
    }

    // Step 9. TT store.
    tt.store(position_key,
             best_move,
             best_value,
             qsearch_tt_depth,
             tt_bound_for_window(best_value, original_alpha, beta),
             search_ply);

    return best_value;
}

// Template definitions live in this translation unit; instantiate the node types we use.
template EvalValue
SearchWorker::alphabeta<NodeType::Pv>(EvalValue, EvalValue, int, PrincipalVariation*, bool);
template EvalValue
SearchWorker::alphabeta<NodeType::NonPv>(EvalValue, EvalValue, int, PrincipalVariation*, bool);
template EvalValue
SearchWorker::quiescence<NodeType::Pv>(EvalValue, EvalValue, PrincipalVariation*);
template EvalValue
SearchWorker::quiescence<NodeType::NonPv>(EvalValue, EvalValue, PrincipalVariation*);
