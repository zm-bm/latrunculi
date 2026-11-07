#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>

#include "core/constants.hpp"
#include "eval/evaluation.hpp"
#include "movegen/moves.hpp"
#include "search/history_learning.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"

namespace search {

namespace {

// Shallow pruning. Both reverse and move-loop futility use the same margins.
constexpr int FutilityMaxDepth         = 3;
constexpr int FutilityMargin[]         = {0, 250, 400, 550};
constexpr int RazorMaxDepth            = 3;
constexpr int RazorMargin[]            = {0, 500, 900, 1800};
constexpr int LateMovePruningDepth     = 2;
constexpr int LateMovePruningMoveCount = 12;

// Null-move reductions.
constexpr int   NullMoveMinDepth        = 3;
constexpr int   NullMoveReductionBase   = 3;
constexpr int   NullMoveReductionMax    = 6;
constexpr int   NullMoveDepthDivisor    = 7;
constexpr Score NullMoveSurplusPerPly   = 2 * eval::pawn.mg;
constexpr int   NullMoveSurplusBonusMax = 2;

// Singular searches.
constexpr int SingularMinDepth       = 8;
constexpr int SingularTtDepthSlack   = 3;
constexpr int SingularPlyLimitFactor = 2;
constexpr int SingularMarginDivisor  = 50;
constexpr int SingularDepthDivisor   = 2;

// Late-move reductions.
constexpr int    LmrMinDepth              = 3;
constexpr int    LmrMinMoveCount          = 4;
constexpr double LmrQuietOffset           = 1.25;
constexpr double LmrCaptureOffset         = 0.75;
constexpr double LmrQuietDivisor          = 2.5;
constexpr double LmrCaptureDivisor        = 3.3;
constexpr double LmrPvScale               = 0.7;
constexpr double LmrKillerScale           = 0.8;
constexpr int    CaptureLmrMinDepth       = 4;
constexpr int    CaptureLmrHistoryDivisor = 8192;
constexpr double CaptureLmrMaxAdjustment  = 1.0;

// Quiescence.
constexpr int   QuiescenceTtDepth = 0;
constexpr Score QCaptureMargin    = 300;

// Null-search roots suppress early pruning and singular searches, but retain
// ordinary TT and move-loop behavior. A loaded position without history is not a null edge.
bool after_null_move(const Board& board, int ply) noexcept {
    return ply > 0 && board.previous_move().is_null() && board.can_unmake();
}

// Called after Board::is_draw(). Checkmate takes precedence over the fifty-move rule.
Score draw_or_mate_score(const Board& board, int ply) {
    if (board.halfmove_clock() >= 100 && board.is_check()) {
        return movegen::UnscoredMoves(board).next().is_null() ? -scores::mate + ply : scores::draw;
    }
    return scores::draw;
}

} // namespace

// Recursive main search: alpha-beta for non-PV nodes, PVS for PV nodes.
template <Worker::NodeType Node>
Score Worker::alphabeta(Score alpha, Score beta, int depth, SearchFrame* frame) {
    const int ply = frame->ply;
    /*
     * PV reset and stop checks.
     * Clear the PV and check whether the search should stop.
     */
    if (frame->pv_enabled)
        frame->pv.clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    /*
     * Draws and ply limit.
     * Return for a draw or the ply limit before entering the main search.
     */
    const bool drawn = board.is_draw(ply);

    if (drawn) {
        increment_nodes();
        stats.node(ply);
        return draw_or_mate_score(board, ply);
    }

    if (ply >= engine::max_search_ply) {
        increment_nodes();
        stats.node(ply);
        return eval::evaluate(board);
    }

    /*
     * Frame setup and quiescence.
     * Set up the frame, then use quiescence at depth zero or below. It counts its own nodes.
     */
    const Move excluded_move = frame->excluded_move;
    // An excluded search tests alternatives with restricted pruning and no TT cutoffs,
    // TT stores, or history learning at this node.
    const bool excluded_search = !excluded_move.is_null();
    assert(!excluded_search || depth > 0);
    frame->static_eval.reset();
    frame->real_move = !board.previous_move().is_null();
    if (depth <= 0)
        return quiescence<Node>(alpha, beta, frame);

    increment_nodes();
    stats.node(ply);

    /*
     * Mate-distance limits and TT lookup.
     * Limit the window to possible mate scores, then check for a usable TT bound.
     */
    alpha = std::max(alpha, -scores::mate + ply);
    beta  = std::min(beta, scores::mate - ply - 1);
    if (alpha >= beta)
        return alpha;

    const Score       original_alpha = alpha;
    const PositionKey tt_key         = board.tt_key();
    Move              tt_move        = NULL_MOVE;

    stats.main_tt_probe(ply);
    const auto  tt_probe  = tt.probe(tt_key);
    const auto& tt_record = tt_probe.record;
    if (tt_record) {
        stats.main_tt_hit(ply);

        const TTRecord& record   = *tt_record;
        const Score     tt_score = record.score_at_ply(ply);
        // A TT score has no continuation; PV nodes must search.
        if (!excluded_search && Node == NodeType::NonPv
            && record.can_cutoff(tt_score, depth, alpha, beta)) {
            stats.main_tt_cutoff(ply);
            return tt_score;
        }

        tt_move = record.move;
    }

    /*
     * Static evaluation and early pruning.
     * Non-PV nodes keep an evaluation for pruning and TT storage; PV nodes leave it unset.
     */
    const bool  in_check       = board.is_check();
    const Color side           = board.side_to_move();
    bool        quiet_futility = false;
    Score       static_eval    = TTRecord::no_static_eval;

    bool declining = false;
    if constexpr (Node == NodeType::NonPv) {
        const bool after_null = after_null_move(board, ply);
        static_eval = tt_record && tt_record->has_static_eval() ? tt_record->static_eval
                                                                : eval::evaluate(board);
        if (!in_check) {
            // Keep evaluations outside check in the frame for LMR's two-ply comparison.
            frame->static_eval = static_eval;
            declining          = ply >= 2 && frame->real_move && (frame - 1)->real_move
                     && (frame - 2)->static_eval && static_eval < *(frame - 2)->static_eval;
        }

        // At shallow depth, try quiescence when the evaluation is far below alpha.
        const bool razor_candidate = !excluded_search && !after_null && !in_check
                                  && depth <= RazorMaxDepth && tt_move.is_null();
        if (razor_candidate && static_eval + RazorMargin[depth] <= alpha) {
            stats.razor_try(ply);
            const bool pv_enabled = frame->pv_enabled;
            frame->pv_enabled     = false;
            const Score value     = quiescence<NodeType::NonPv>(alpha - 1, alpha, frame);
            frame->pv_enabled     = pv_enabled;
            if (stop_requested())
                return alpha;
            if (value < alpha) {
                stats.razor_cutoff(ply);
                return value;
            }
        }

        // Skip reverse futility and null-move pruning when a deep TT bound is below beta.
        const bool tt_upper_veto = tt_record && tt_record->depth >= depth
                                && tt_record->bound == TTBound::UpperBound
                                && tt_record->score_at_ply(ply) < beta;

        // Reverse futility: return when the evaluation remains above beta after its margin.
        const bool reverse_futility_candidate =
            !excluded_search && !after_null && !in_check && depth <= FutilityMaxDepth
            && beta > -scores::mate_bound && beta < scores::mate_bound;
        if (reverse_futility_candidate && board.non_pawn_material(side) > eval::piece(ROOK).mg
            && !tt_upper_veto && static_eval - FutilityMargin[depth] >= beta)
            return static_eval;

        // Null-move pruning: try a reduced search after passing the turn.
        if (!excluded_search && !after_null && !in_check && depth >= NullMoveMinDepth
            && board.non_pawn_material(side) > eval::piece(ROOK).mg && !tt_upper_veto) {
            stats.null_move_try(ply);

            const Score positive_surplus = std::max(static_eval - beta, Score{0});
            const int   surplus_bonus =
                std::min(int(positive_surplus / NullMoveSurplusPerPly), NullMoveSurplusBonusMax);
            const int reduction =
                std::min({depth,
                          NullMoveReductionMax,
                          NullMoveReductionBase + depth / NullMoveDepthDivisor + surplus_bonus});

            board.make_null(prefetch_child_tt);
            (frame + 1)->singular_search_active = frame->singular_search_active;
            (frame + 1)->pv_enabled             = false;
            const Score value =
                -alphabeta<NodeType::NonPv>(-beta, -beta + 1, depth - reduction, frame + 1);
            // Restore the parent position before checking the result or stop request.
            board.unmake_null();

            if (stop_requested())
                return alpha;
            if (value >= beta) {
                stats.null_move_cutoff(ply);
                return value;
            }
        }

        // Prepare shallow futility pruning. The move loop performs the actual skip.
        quiet_futility = !excluded_search && depth <= FutilityMaxDepth && !in_check
                      && alpha > -scores::mate_bound && alpha < scores::mate_bound
                      && static_eval + FutilityMargin[depth] <= alpha;
    }

    /*
     * Singular search.
     * Search without the TT move to decide whether to extend it or take a multicut.
     */
    bool singular_extension = false;

    const bool within_singular_limits = depth >= SingularMinDepth && ply > 0
                                     && ply < SingularPlyLimitFactor * root_iteration_depth
                                     && ply + 1 < engine::max_search_ply;

    // Require a sufficiently deep TT bound and room to extend this search path.
    if (tt_record && within_singular_limits && !excluded_search && !after_null_move(board, ply)
        && tt_record->depth >= depth - SingularTtDepthSlack
        && (tt_record->bound == TTBound::Exact || tt_record->bound == TTBound::LowerBound)) {
        const Score tt_score = tt_record->score_at_ply(ply);
        const Score singular_beta =
            tt_score - std::max(1, depth * eval::pawn.mg / SingularMarginDivisor);
        const int singular_depth = (depth - 1) / SingularDepthDivisor;

        const bool non_mate_scores = std::abs(tt_score) < scores::mate_bound
                                  && singular_beta > -scores::mate_bound
                                  && singular_beta < scores::mate_bound;
        if (non_mate_scores && !tt_move.is_null() && board.is_pseudo_legal(tt_move)
            && board.is_legal_pseudo_move(tt_move)) {
            // Reuse this ply without PV output, then restore its search metadata.
            const auto saved_eval         = frame->static_eval;
            const bool saved_real_move    = frame->real_move;
            const bool pv_enabled         = frame->pv_enabled;
            const bool was_active         = frame->singular_search_active;
            frame->excluded_move          = tt_move;
            frame->pv_enabled             = false;
            frame->singular_search_active = true;
            const Score singular_score =
                alphabeta<NodeType::NonPv>(singular_beta - 1, singular_beta, singular_depth, frame);
            frame->static_eval            = saved_eval;
            frame->real_move              = saved_real_move;
            frame->excluded_move          = excluded_move;
            frame->pv_enabled             = pv_enabled;
            frame->singular_search_active = was_active;

            if (stop_requested())
                return alpha;

            if (singular_score < singular_beta) {
                singular_extension = true;
            } else if constexpr (Node == NodeType::NonPv) {
                const bool non_mate_window =
                    alpha > -scores::mate_bound && alpha < scores::mate_bound
                    && beta > -scores::mate_bound && beta < scores::mate_bound;

                // Multicuts also require the singular window's lower edge outside mate ranges.
                if (!frame->singular_search_active && !in_check && non_mate_window
                    && singular_beta - 1 > -scores::mate_bound && singular_beta >= beta)
                    return stop_requested() ? alpha : singular_beta;
            }
        }
    }
    if (stop_requested())
        return alpha;

    /*
     * Move preparation.
     * Set up ordered moves and history learning for this node.
     */
    int   move_count = 0;
    Score best_value = -scores::inf;
    Move  best_move  = NULL_MOVE;

    const auto context = history.context_for(board, ply, tt_move);
    auto       moves   = movegen::MainSearchMoves(board, history, context);

    SearchFrame* child = frame + 1;
    child->pv.clear();
    child->singular_search_active = frame->singular_search_active;
    // Excluded searches neither train history nor contribute learning instrumentation.
    std::optional<HistoryLearning> learning;
    if (!excluded_search)
        learning.emplace(board, history, context, depth, stats);

    for (Move move = moves.next(); !move.is_null(); move = moves.next()) {
        if (move == excluded_move)
            continue;

        ++move_count;
        const bool first_legal = move_count == 1;

        /*
         * Move pruning and reductions.
         * Read parent history before making the move, then check whether it gives check.
         */
        const bool is_promotion      = move.type() == MOVE_PROM;
        const bool is_capture        = board.is_capture(move);
        const bool is_quiet          = !is_capture && !is_promotion;
        const bool is_ordinary_quiet = is_quiet && move.type() == BASIC_MOVE;
        const bool is_killer         = is_quiet && history.is_killer(move, ply);
        const bool is_counter        = is_quiet && move == context.counter_move;

        // LMR uses current history, read before making the move. Ordering retains its snapshot.
        int raw_capture_history = 0;
        if (is_capture && !is_promotion && !in_check)
            raw_capture_history = history.capture_score(board, move);
        bool history_lmp_candidate = false;
        if constexpr (Node == NodeType::NonPv) {
            const bool late_quiet_pruning_node =
                !excluded_search && depth == LateMovePruningDepth
                && move_count > LateMovePruningMoveCount && !quiet_futility && !in_check
                && best_value > -scores::mate_bound && alpha > -scores::mate_bound
                && beta < scores::mate_bound;
            if (late_quiet_pruning_node && board.non_pawn_material(side) > 0) {
                const bool unprotected_quiet =
                    is_ordinary_quiet && move != tt_move && !is_killer && !is_counter;
                history_lmp_candidate =
                    unprotected_quiet && history.quiet_score(context, board, move) < 0;
            }
        }

        const int child_depth = depth - 1 + (move == tt_move && singular_extension);
        board.make(move, prefetch_child_tt);

        // Child position: checking status gates pruning and reduction.
        const bool gives_check = board.is_check();
        if (quiet_futility && !first_legal && is_quiet && !gives_check) {
            board.unmake();

            moves.skip_nonchecking_quiets();
            stats.futility_skip(ply);
            continue;
        }

        // History-gated late-move pruning.
        if (history_lmp_candidate && !gives_check) {
            board.unmake();

            continue;
        }

        // Reduce late moves unless they promote, give check, or escape check.
        int  reduction          = 0;
        int  base_reduction     = 0;
        bool capture_adjustment = false;
        if (depth >= LmrMinDepth && move_count >= LmrMinMoveCount && !is_promotion && !in_check
            && !gives_check) {
            const double offset         = is_quiet ? LmrQuietOffset : LmrCaptureOffset;
            const double divisor        = is_quiet ? LmrQuietDivisor : LmrCaptureDivisor;
            double fractional_reduction = offset + std::log(depth) * std::log(move_count) / divisor;

            if constexpr (Node == NodeType::Pv)
                fractional_reduction *= LmrPvScale;
            if (is_killer)
                fractional_reduction *= LmrKillerScale;

            // Keep at least one main-search ply after the move.
            reduction = std::clamp(static_cast<int>(fractional_reduction), 1, depth - 2);
            if constexpr (Node == NodeType::NonPv) {
                if (declining && is_quiet && !is_killer && reduction < depth - 2)
                    ++reduction;
            }
            base_reduction = reduction;

            if constexpr (Node == NodeType::NonPv) {
                // Adjust captures before rounding; retain the integer base for instrumentation.
                if (!is_quiet && move != tt_move && depth >= CaptureLmrMinDepth) {
                    const double history_adjustment =
                        -static_cast<double>(raw_capture_history) / CaptureLmrHistoryDivisor;
                    const double adjustment = std::clamp(
                        history_adjustment, -CaptureLmrMaxAdjustment, CaptureLmrMaxAdjustment);
                    reduction = std::clamp(
                        static_cast<int>(fractional_reduction + adjustment), 0, depth - 2);
                    capture_adjustment = true;
                }
            }
        }
        stats.capture_lmr(capture_adjustment, raw_capture_history, base_reduction, reduction);

        /*
         * Recursive search.
         * Scout reduced moves and later PV moves before a full-depth search.
         */
        const bool scout = reduction > 0 || (Node == NodeType::Pv && !first_legal);
        Score      value;
        if (scout) {
            if (reduction > 0)
                stats.lmr_try(ply);
            child->pv_enabled = false;
            value = -alphabeta<NodeType::NonPv>(-alpha - 1, -alpha, child_depth - reduction, child);
        }

        if (!scout || (!stop_requested() && value > alpha)) {
            if (reduction > 0) {
                stats.lmr_research(ply);
                stats.capture_research(capture_adjustment, base_reduction != reduction);
            }
            if constexpr (Node == NodeType::Pv) {
                if (scout) {
                    stats.pvs_research(child->ply);
                    child->pv.clear();
                }
            }
            child->pv_enabled = Node == NodeType::Pv && frame->pv_enabled;
            value             = -alphabeta<Node>(-beta, -alpha, child_depth, child);
        }

        /*
         * Move results.
         * Restore the parent position before stop checks, learning, PV updates, or TT stores.
         */
        board.unmake();

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            if (learning)
                learning->learn_from_cutoff(move, is_capture);

            if (frame->pv_enabled)
                frame->pv.update(move, child->pv);

            stats.beta_cutoff(ply, move_count);
            if (!excluded_search)
                tt.store(tt_probe.writer,
                         tt_key,
                         move,
                         value,
                         depth,
                         TTBound::LowerBound,
                         ply,
                         static_eval);
            return value;
        }

        if (learning)
            learning->record_non_cutoff(move, is_capture, is_killer);
        if (value > best_value) {
            best_value = value;
            best_move  = move;

            if (value > alpha) {
                alpha = value;
                if (frame->pv_enabled)
                    frame->pv.update(move, child->pv);
            }
        }
    }

    /*
     * Final node result.
     * Handle an empty move list, otherwise store the best score and its bound.
     */
    if (move_count == 0) {
        // Excluding the only move does not make the actual position terminal.
        if (excluded_search)
            return original_alpha;
        best_value = in_check ? -scores::mate + ply : scores::draw;
        tt.store(tt_probe.writer,
                 tt_key,
                 NULL_MOVE,
                 best_value,
                 depth,
                 TTBound::Exact,
                 ply,
                 static_eval);
        return best_value;
    }

    if (!excluded_search)
        tt.store(tt_probe.writer,
                 tt_key,
                 best_move,
                 best_value,
                 depth,
                 tt_bound_for_window(best_value, original_alpha, beta),
                 ply,
                 static_eval);

    return best_value;
}

// Quiescence search for tactical depth-zero nodes.
template <Worker::NodeType Node, bool UseTt>
Score Worker::quiescence(Score alpha, Score beta, SearchFrame* frame) {
    const int ply = frame->ply;
    /*
     * PV reset and stop checks.
     * Clear the PV and check whether the search should stop.
     */
    if (frame->pv_enabled)
        frame->pv.clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    /*
     * Node counting and terminal exits.
     * Count this node, then return for a draw or the ply limit.
     */
    increment_nodes();
    stats.qnode(ply);

    if (board.is_draw(ply))
        return draw_or_mate_score(board, ply);

    if (ply >= engine::max_search_ply)
        return eval::evaluate(board);

    /*
     * TT lookup.
     * Check for a usable bound and retain the TT move for ordering.
     */
    const Score       original_alpha = alpha;
    const PositionKey tt_key         = board.tt_key();
    Move              tt_move        = NULL_MOVE;
    TTProbe           tt_probe;
    const auto&       tt_record = tt_probe.record;

    if constexpr (UseTt) {
        stats.q_tt_probe(ply);
        tt_probe = tt.probe(tt_key);
        if (tt_record) {
            stats.q_tt_hit(ply);

            const Score tt_score = tt_record->score_at_ply(ply);
            if (Node == NodeType::NonPv
                && tt_record->can_cutoff(tt_score, QuiescenceTtDepth, alpha, beta)) {
                stats.q_tt_cutoff(ply);
                return tt_score;
            }

            tt_move = tt_record->move;
        }
    }

    /*
     * Stand pat.
     * Outside check, use the static evaluation to try a cutoff or raise alpha.
     */
    const bool in_check    = board.is_check();
    int        move_count  = 0;
    Score      best_value  = -scores::inf;
    Move       best_move   = NULL_MOVE;
    Score      static_eval = TTRecord::no_static_eval;

    if (!in_check) {
        static_eval = tt_record && tt_record->has_static_eval() ? tt_record->static_eval
                                                                : eval::evaluate(board);
        best_value  = static_eval;
        if (best_value >= beta) {
            if constexpr (UseTt) {
                tt.store(tt_probe.writer,
                         tt_key,
                         NULL_MOVE,
                         best_value,
                         QuiescenceTtDepth,
                         TTBound::LowerBound,
                         ply,
                         static_eval);
            }
            return best_value;
        }
        if (best_value > alpha)
            alpha = best_value;
    }

    // Search tactical moves, or all legal evasions when in check.
    auto moves = movegen::QuiescenceMoves(board, history, tt_move);
    SearchFrame* child = frame + 1;
    child->pv.clear();
    child->singular_search_active = frame->singular_search_active;

    for (Move move = moves.next(); !move.is_null(); move = moves.next()) {
        ++move_count;

        /*
         * Capture pruning.
         * Use evaluation material to estimate whether a capture can improve alpha.
         */
        if (!in_check && move != tt_move && move.type() == BASIC_MOVE && alpha > -scores::mate_bound
            && beta < scores::mate_bound) {
            const PieceType victim = board.captured_piece_type(move);
            const auto      price  = eval::piece(victim);
            const Score     capture_gain_estimate =
                static_eval + std::max(price.mg, price.eg) + QCaptureMargin;
            if (capture_gain_estimate < alpha) {
                const bool material_allows_capture_pruning =
                    board.non_pawn_material(board.side_to_move()) > eval::rook.mg
                    && board.non_pawn_material(~board.side_to_move())
                               - (victim == PAWN ? 0 : price.mg)
                           > eval::rook.mg;
                if (material_allows_capture_pruning && !board.gives_check(move)) {
                    // Keep a fail-low estimate; leave alpha and the PV unchanged.
                    best_value = std::max(best_value, capture_gain_estimate);
                    continue;
                }
            }
        }

        // Search the child position.
        board.make(move, UseTt ? prefetch_child_tt : nullptr);

        child->pv_enabled = frame->pv_enabled;
        const Score value = -quiescence<Node, UseTt>(-beta, -alpha, child);

        /*
         * Move results.
         * Restore the parent position before stop checks, PV updates, or TT stores.
         */
        board.unmake();

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            if (frame->pv_enabled)
                frame->pv.update(move, child->pv);
            stats.beta_cutoff(ply, move_count);
            if constexpr (UseTt) {
                tt.store(tt_probe.writer,
                         tt_key,
                         move,
                         value,
                         QuiescenceTtDepth,
                         TTBound::LowerBound,
                         ply,
                         static_eval);
            }
            return value;
        }

        if (value > best_value) {
            best_value = value;
            best_move  = move;
            if (value > alpha) {
                alpha = value;
                if (frame->pv_enabled)
                    frame->pv.update(move, child->pv);
            }
        }
    }

    /*
     * Final node result.
     * Handle checkmate, otherwise store the best score and its bound.
     */
    if (in_check && move_count == 0) {
        best_value = -scores::mate + ply;
        if constexpr (UseTt) {
            tt.store(tt_probe.writer,
                     tt_key,
                     NULL_MOVE,
                     best_value,
                     QuiescenceTtDepth,
                     TTBound::Exact,
                     ply,
                     static_eval);
        }
        return best_value;
    }

    if constexpr (UseTt) {
        tt.store(tt_probe.writer,
                 tt_key,
                 best_move,
                 best_value,
                 QuiescenceTtDepth,
                 tt_bound_for_window(best_value, original_alpha, beta),
                 ply,
                 static_eval);
    }

    return best_value;
}

bool Worker::settle(Board& position) {
    configure_search(position, Limits{}, SearchClock::now());
    history.clear();

    SearchFrame* frame            = &frames[0];
    frame->pv_enabled             = true;
    frame->singular_search_active = false;
    const Score value = quiescence<NodeType::Pv, false>(-scores::inf, scores::inf, frame);
    if (std::abs(value) >= scores::mate_bound)
        return false;

    for (int index = 0; index < frame->pv.size(); ++index)
        board.make(frame->pv.move_at(index));

    if (board.is_draw())
        return false;

    if (!movegen::UnscoredMoves(board).next().is_null()) {
        position = board;
        return true;
    }
    return false;
}

// Template definitions live in this translation unit; instantiate the node types we use.
template Score Worker::alphabeta<Worker::NodeType::Pv>(Score, Score, int, Worker::SearchFrame*);
template Score Worker::alphabeta<Worker::NodeType::NonPv>(Score, Score, int, Worker::SearchFrame*);
template Score Worker::quiescence<Worker::NodeType::Pv, true>(Score, Score, Worker::SearchFrame*);
template Score
Worker::quiescence<Worker::NodeType::NonPv, true>(Score, Score, Worker::SearchFrame*);

} // namespace search
