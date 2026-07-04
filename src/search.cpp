#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "defs.hpp"
#include "evaluator.hpp"
#include "move_picker.hpp"
#include "search_worker.hpp"
#include "tt.hpp"

namespace {

// Aspiration-window defaults.
constexpr int AspirationWindow = 50;
constexpr int SearchInf        = static_cast<int>(INF_VALUE);

// Null-move pruning defaults.
constexpr int NullMoveReductionBase = 3;
constexpr int NullMoveReductionDeep = 4;
constexpr int NullMoveDeepDepth     = 6;

// Razoring and futility defaults.
constexpr int RazorFutilityMaxDepth = 3;
constexpr int RazorMargin[]         = {0, 500, 900, 1800};
constexpr int FutilityMargin[]      = {0, 250, 400, 550};

// Late-move reduction defaults.
constexpr int LmrMinDepth     = 3;
constexpr int LmrMinMoveCount = 3;

// Quiet-history malus defaults.
constexpr int QuietMalusMinDepth  = 4;
constexpr int QuietMalusMinFailed = 2;
constexpr int QuietMalusDivisor   = 2;

// Apply the PV/non-PV TT cutoff policy.
template <NodeType Node>
bool tt_cutoff_allowed(
    const TT_Record& record, int adjusted_score, int depth, int alpha, int beta) {
    if constexpr (Node == PV)
        return record.depth >= depth && record.flag == TT_Flag::Exact;

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
    if constexpr (Node == PV)
        r *= 0.7;
    if (is_killer)
        r *= 0.8;

    // Do not extend or drop straight into qsearch.
    return std::clamp(static_cast<int>(r), 1, depth - 2);
}

struct SearchedMoves {
    static constexpr int Capacity = 32;

    bool add(Move move) {
        if (count_ >= Capacity)
            return false;

        move_bits_[count_++] = move.bits;
        return true;
    }

    int size() const { return count_; }

    template <typename Fn>
    void for_each(Fn fn) const {
        for (int i = 0; i < count_; ++i) {
            Move move;
            move.bits = move_bits_[i];
            fn(move);
        }
    }

private:
    uint16_t move_bits_[Capacity];
    int      count_{0};
};

} // namespace

// Main root search driver.
int SearchWorker::search_root() {
    // Terminal root: return immediately when no legal root move exists.
    if (root_lines.empty()) {
        root_result = terminal_root_result();
        return root_result.value;
    }

    // Iterative deepening searches one completed depth at a time.
    for (int depth = 1; depth <= options.depth && !stop_requested(); ++depth) {
        if (!search_root_depth(depth, root_result.value))
            break;
    }

    return root_result.value;
}

// Root aspiration loop for a single depth.
bool SearchWorker::search_root_depth(int depth, int previous_value) {
    int delta = AspirationWindow;
    int alpha = std::max(previous_value - delta, -SearchInf);
    int beta  = std::min(previous_value + delta, SearchInf);

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
        const int value = best_line.value;
        assert(value > -SearchInf && value < SearchInf);

        if (value <= alpha) {
            // Fail low: widen the lower bound and re-search.
            stats.aspiration_fail_low();
            alpha = std::max(alpha - delta, -SearchInf);
            stats.aspiration_research();
        } else if (value >= beta) {
            // Fail high: widen the upper bound and re-search.
            stats.aspiration_fail_high();
            beta = std::min(beta + delta, SearchInf);
            stats.aspiration_research();
        } else {
            // Window hit: accept and publish the completed depth.
            root_result = best_line;
            update_root_snapshot();
            return true;
        }

        // Increase retry width after each aspiration miss.
        delta = delta >= SearchInf / 2 ? SearchInf : delta * 2;
    }

    return false;
}

// Fixed-window root pass. Caller owns attempt reset and result ordering.
bool SearchWorker::search_root_window(int depth, int alpha, int beta) {
    assert(!root_lines.empty());

    int  move_count    = 0;
    bool pv_move_found = false;

    // Preserve caller order for iterative deepening and aspiration retries.
    for (RootLine& line : root_lines) {
        assert(line.has_root_move());
        const Move root_move = line.root_move;
        assert(board.is_legal_generated_move(root_move));

        ++move_count;

        board.make(root_move, position_states.child(ply));
        ++ply;

        // Root PVS searches full-window until a root PV is established.
        // Scout later root moves and re-search only strict alpha improvements.
        PrincipalVariation child_pv;
        int                value;
        if (move_count == 1 || !pv_move_found) {
            value = -alphabeta<PV>(-beta, -alpha, depth - 1, &child_pv);
        } else {
            value = -alphabeta<NON_PV>(-alpha - 1, -alpha, depth - 1);
            if (!stop_requested() && value > alpha) {
                stats.pvs_research(ply);
                child_pv.clear();
                value = -alphabeta<PV>(-beta, -alpha, depth - 1, &child_pv);
            }
        }

        board.unmake(position_states.parent(ply));
        --ply;

        // Do not record partial root-line state after a stop.
        if (stop_requested())
            return false;

        line.complete(depth, value, child_pv);

        // Let aspiration handle the fail-high window miss.
        if (value >= beta)
            return true;

        if (value > alpha) {
            // A new root move improved alpha.
            alpha         = value;
            pv_move_found = true;
        } else if (move_count > 1 && pv_move_found && value == alpha && alpha > -INF_VALUE) {
            // Keep the full-window alpha raiser ordered first on scout ties.
            line.value = alpha - 1;
        }
    }

    return true;
}

// Recursive main search: alpha-beta for non-PV nodes, PVS for PV nodes.
template <NodeType Node>
int SearchWorker::alphabeta(int alpha, int beta, int depth, PrincipalVariation* pv, bool can_null) {
    // Step 1. PV and Abort Checks. Clear caller PV and honor pending stops.
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    // Step 2. Early Exit Conditions. Resolve draws, max ply, and qsearch entry.
    const bool drawn = board.is_draw(ply);

    if (drawn) {
        increment_nodes();
        stats.node(ply);
        return DRAW_VALUE;
    }

    if (ply >= MAX_SEARCH_PLY) {
        increment_nodes();
        stats.node(ply);
        return evaluate(board);
    }

    if (depth <= 0)
        return quiescence<Node>(alpha, beta, pv);

    increment_nodes();
    stats.node(ply);

    // Step 3. Mate Distance Pruning. Clamp scores that cannot improve this line.
    alpha = std::max(alpha, -MATE_VALUE + ply);
    beta  = std::min(beta, MATE_VALUE - ply - 1);
    if (alpha >= beta)
        return alpha;

    const int      original_alpha = alpha;
    const uint64_t position_key   = board.key();
    Move           tt_move        = NULL_MOVE;

    // Step 4. Transposition Table. Probe for a cutoff or a hash move.
    stats.main_tt_probe(ply);
    const auto tt_record = tt.probe(position_key);
    if (tt_record) {
        stats.main_tt_hit(ply);

        const TT_Record& record   = *tt_record;
        const int        tt_score = record.score_at_ply(ply);
        if (tt_cutoff_allowed<Node>(record, tt_score, depth, alpha, beta)) {
            stats.main_tt_cutoff(ply);
            return tt_score;
        }

        tt_move = record.move;
    }

    const bool  in_check = board.is_check();
    const Color c        = board.side_to_move();
    bool        futility = false;

    if constexpr (Node == NON_PV) {
        // Step 5. Razoring. At shallow fail-low nodes, verify with qsearch.
        const int static_eval = evaluate(board);
        if (can_null && !in_check && depth <= RazorFutilityMaxDepth && tt_move.is_null() &&
            static_eval + RazorMargin[depth] <= alpha) {
            stats.razor_try(ply);
            const int value = quiescence<NON_PV>(alpha - 1, alpha);
            if (stop_requested())
                return alpha;
            if (value < alpha) {
                stats.razor_cutoff(ply);
                return value;
            }
        }

        // Step 6. Null Move Pruning. If passing still maintains beta, prune early.
        // Skip NMP when a depth-sufficient TT upper bound suggests it will fail low.
        const int reduction =
            depth > NullMoveDeepDepth ? NullMoveReductionDeep : NullMoveReductionBase;
        const bool tt_upper_veto = tt_record && tt_record->depth >= depth &&
                                   tt_record->flag == TT_Flag::Upperbound &&
                                   tt_record->score_at_ply(ply) < beta;
        if (can_null && !in_check && depth >= reduction && board.nonPawnMaterial(c) > ROOK_MG &&
            !tt_upper_veto) {
            stats.null_move_try(ply);

            board.make_null(position_states.child(ply));
            ++ply;
            const int value =
                -alphabeta<NON_PV>(-beta, -beta + 1, depth - reduction, nullptr, false);
            board.unmake_null(position_states.parent(ply));
            --ply;

            if (stop_requested())
                return alpha;
            if (value >= beta) {
                stats.null_move_cutoff(ply);
                return value;
            }
        }

        // Prepare shallow futility pruning. The move loop performs the actual skip.
        futility = depth <= RazorFutilityMaxDepth && !in_check && alpha > -MATE_BOUND &&
                   alpha < MATE_BOUND && static_eval + FutilityMargin[depth] <= alpha;
    }

    int  move_count = 0;
    int  best_value = -INF_VALUE;
    Move best_move  = NULL_MOVE;

    const auto         ctx    = MoveOrdering::make_context(board);
    MovePicker         picker = MovePicker::main_search(board, ordering, ctx, ply, tt_move);
    PrincipalVariation child_pv;
    SearchedMoves      searched_quiets;

    const bool quiet_malus_eligible = depth >= QuietMalusMinDepth && !in_check;
    if (quiet_malus_eligible)
        stats.quiet_malus_eligible_node(depth);

    // Step 8. Move Loop. Search ordered legal moves until cutoff or exhaustion.
    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (!board.is_legal_generated_move(move))
            continue;

        ++move_count;
        const bool first_legal = move_count == 1;

        const bool is_promotion = move.type() == MOVE_PROM;
        const bool is_capture   = board.is_capture(move);
        const bool is_quiet     = !is_capture && !is_promotion;
        const bool is_killer    = is_quiet && ordering.is_killer(move, ply);
        board.make(move, position_states.child(ply));
        ++ply;

        const bool gives_check = board.is_check();
        if (futility && !first_legal && is_quiet && !gives_check) {
            // Step 9. Futility Pruning. Skip late quiet moves that cannot raise alpha.
            board.unmake(position_states.parent(ply));
            --ply;
            picker.skip_quiet_moves();
            stats.futility_skip(ply);
            continue;
        }

        // Step 10. Late Move Reductions. Search late moves at reduced depth first.
        // If the reduced search beats alpha, research the move at full depth.
        int       value;
        const int reduction = lmr_reduction<Node>(
            depth, move_count, is_quiet, is_promotion, in_check, gives_check, is_killer);
        if (reduction > 0) {
            stats.lmr_try(ply - 1);
            value = -alphabeta<NON_PV>(-alpha - 1, -alpha, depth - 1 - reduction, nullptr, true);
            if (!stop_requested() && value > alpha) {
                stats.lmr_research(ply - 1);
                if constexpr (Node == PV) {
                    stats.pvs_research(ply);
                    child_pv.clear();
                    value =
                        -alphabeta<PV>(-beta, -alpha, depth - 1, pv ? &child_pv : nullptr, true);
                } else {
                    value = -alphabeta<NON_PV>(-beta, -alpha, depth - 1, nullptr, true);
                }
            }
        } else {
            // Step 11. Principal Variation Search. Scout later PV moves before re-search.
            if constexpr (Node == NON_PV) {
                value = -alphabeta<NON_PV>(-beta, -alpha, depth - 1, nullptr, true);
            } else if (move_count == 1) {
                value = -alphabeta<PV>(-beta, -alpha, depth - 1, pv ? &child_pv : nullptr, true);
            } else {
                value = -alphabeta<NON_PV>(-alpha - 1, -alpha, depth - 1, nullptr, true);
                if (!stop_requested() && value > alpha) {
                    stats.pvs_research(ply);
                    child_pv.clear();
                    value =
                        -alphabeta<PV>(-beta, -alpha, depth - 1, pv ? &child_pv : nullptr, true);
                }
            }
        }

        board.unmake(position_states.parent(ply));
        --ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Step 12. Beta Cutoff. Return fail-soft value and store a lower bound.
            if (is_quiet) {
                stats.quiet_cutoff(depth);
                ordering.update_quiet_refutations(ctx, move, ply);
                ordering.reward_quiet(ctx, board, move, depth);
                if (quiet_malus_eligible && searched_quiets.size() >= QuietMalusMinFailed) {
                    searched_quiets.for_each([&](Move quiet) {
                        ordering.penalize_quiet(ctx, board, quiet, depth, QuietMalusDivisor);
                        stats.quiet_malus_update(depth);
                    });
                }
            }

            if (pv)
                pv->update(move, child_pv);

            stats.beta_cutoff(ply, move_count);
            tt.store_search(position_key, move, value, depth, TT_Flag::Lowerbound, ply);
            return value;
        }

        if (quiet_malus_eligible && is_quiet && !(move == tt_move) && !is_killer) {
            if (searched_quiets.add(move))
                stats.quiet_malus_failed_quiet(depth);
        }
        // Step 13. Best Move Update. Raise alpha and PV when this move improves the node.
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

    // Step 14. Mate and Stalemate Detection. No legal moves ends the node.
    if (move_count == 0) {
        best_value = in_check ? -MATE_VALUE + ply : DRAW_VALUE;
        tt.store_search(position_key, NULL_MOVE, best_value, depth, TT_Flag::Exact, ply);
        return best_value;
    }

    // Step 15. Store Result. Use the original window to classify the bound.
    tt.store_search(position_key,
                    best_move,
                    best_value,
                    depth,
                    tt_bound_for_window(best_value, original_alpha, beta),
                    ply);

    return best_value;
}

// Quiescence search for tactical depth-zero nodes.
template <NodeType Node>
int SearchWorker::quiescence(int alpha, int beta, PrincipalVariation* pv) {
    // Step 1. PV and Abort Checks. Clear caller PV and honor pending stops.
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    increment_nodes();
    stats.qnode(ply);

    // Step 2. Early Exit Conditions. Qsearch is below root, so draws may return.
    if (board.is_draw(ply))
        return DRAW_VALUE;

    if (ply >= MAX_SEARCH_PLY)
        return evaluate(board);

    constexpr int  qsearch_tt_depth = 0;
    const int      original_alpha   = alpha;
    const uint64_t position_key     = board.key();
    Move           tt_move          = NULL_MOVE;

    // Step 3. Transposition Table. Use depth-0 records with the PV cutoff policy.
    stats.q_tt_probe(ply);
    if (auto record = tt.probe(position_key)) {
        stats.q_tt_hit(ply);

        const int tt_score = record->score_at_ply(ply);
        if (tt_cutoff_allowed<Node>(*record, tt_score, qsearch_tt_depth, alpha, beta)) {
            stats.q_tt_cutoff(ply);
            return tt_score;
        }

        tt_move = record->move;
    }

    const bool in_check   = board.is_check();
    int        move_count = 0;
    int        best_value = -INF_VALUE;
    Move       best_move  = NULL_MOVE;

    // Step 4. Stand Pat. Use static eval as the initial lower bound when legal.
    if (!in_check) {
        best_value = evaluate(board);
        if (best_value >= beta) {
            tt.store_search(
                position_key, NULL_MOVE, best_value, qsearch_tt_depth, TT_Flag::Lowerbound, ply);
            return best_value;
        }
        if (best_value > alpha)
            alpha = best_value;
    }

    const auto         ctx    = MoveOrdering::make_context(board, false);
    MovePicker         picker = MovePicker::qsearch(board, ordering, ctx, tt_move);
    PrincipalVariation child_pv;

    // Step 5. Tactical Move Loop. Search noisy moves, or all evasions in check.
    for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
        if (!board.is_legal_generated_move(move))
            continue;

        ++move_count;

        board.make(move, position_states.child(ply));
        ++ply;
        const int value = -quiescence<Node>(-beta, -alpha, pv ? &child_pv : nullptr);
        board.unmake(position_states.parent(ply));
        --ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Step 6. Beta Cutoff. Return fail-soft value and store a lower bound.
            if (pv)
                pv->update(move, child_pv);
            stats.beta_cutoff(ply, move_count);
            tt.store_search(position_key, move, value, qsearch_tt_depth, TT_Flag::Lowerbound, ply);
            return value;
        }

        // Step 7. Best Move Update. Raise alpha and PV when this move improves qsearch.
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

    // Step 8. Checkmate Detection. In-check qsearch must find a legal evasion.
    if (in_check && move_count == 0) {
        best_value = -MATE_VALUE + ply;
        tt.store_search(position_key, NULL_MOVE, best_value, qsearch_tt_depth, TT_Flag::Exact, ply);
        return best_value;
    }

    // Step 9. Store Result. Use the original window to classify the qsearch bound.
    tt.store_search(position_key,
                    best_move,
                    best_value,
                    qsearch_tt_depth,
                    tt_bound_for_window(best_value, original_alpha, beta),
                    ply);

    return best_value;
}

// Template definitions live in this translation unit; instantiate the node types we use.
template int SearchWorker::alphabeta<PV>(int, int, int, PrincipalVariation*, bool);
template int SearchWorker::alphabeta<NON_PV>(int, int, int, PrincipalVariation*, bool);
template int SearchWorker::quiescence<PV>(int, int, PrincipalVariation*);
template int SearchWorker::quiescence<NON_PV>(int, int, PrincipalVariation*);
