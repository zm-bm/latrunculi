#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>

#include "core/constants.hpp"
#include "eval/evaluation.hpp"
#include "movegen/moves.hpp"
#include "search/history_learning.hpp"
#include "search/search_policy.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"

namespace search {

namespace {

// Called after Board::is_draw(). Checkmate takes precedence over the fifty-move rule.
Score draw_or_mate_score(const Board& board, int search_ply) {
    if (board.halfmove_clock() >= 100 && board.is_check()) {
        return movegen::UnscoredMoves(board).next().is_null() ? -scores::mate + search_ply
                                                              : scores::draw;
    }
    return scores::draw;
}

} // namespace

// Recursive main search: alpha-beta for non-PV nodes, PVS for PV nodes.
template <NodeType Node>
Score Worker::alphabeta(Score alpha, Score beta, int depth, PrincipalVariation* pv) {
    /*
     * PV reset and stop checks.
     * Clear the PV, check search limits, and return if a stop was requested.
     */
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    /*
     * Draws and ply limit.
     * Return for a draw or the ply limit before using the current search frame.
     */
    const bool drawn = board.is_draw(search_ply);

    if (drawn) {
        increment_nodes();
        stats.node(search_ply);
        return draw_or_mate_score(board, search_ply);
    }

    if (search_ply >= engine::max_search_ply) {
        increment_nodes();
        stats.node(search_ply);
        return eval::evaluate(board);
    }

    /*
     * Frame setup and quiescence.
     * Set up the frame, then use quiescence at depth zero or below. It counts its own nodes.
     */
    auto&      frame           = frames[search_ply];
    const Move excluded_move   = frame.excluded_move;
    const bool excluded_search = !excluded_move.is_null();
    assert(!excluded_search || depth > 0);
    frame.static_eval.reset();
    frame.real_move = !board.previous_move().is_null();
    if (depth <= 0)
        return quiescence<Node>(alpha, beta, pv);

    increment_nodes();
    stats.node(search_ply);

    /*
     * Mate-distance limits and TT lookup.
     * Limit the window to possible mate scores, then check for a usable TT bound.
     */
    alpha = std::max(alpha, -scores::mate + search_ply);
    beta  = std::min(beta, scores::mate - search_ply - 1);
    if (alpha >= beta)
        return alpha;

    const Score       original_alpha = alpha;
    const PositionKey tt_key         = board.tt_key();
    Move              tt_move        = NULL_MOVE;

    stats.main_tt_probe(search_ply);
    const auto  tt_probe  = tt.probe(tt_key);
    const auto& tt_record = tt_probe.record;
    if (tt_record) {
        stats.main_tt_hit(search_ply);

        const TTRecord& record   = *tt_record;
        const Score     tt_score = record.score_at_ply(search_ply);
        if (!excluded_search
            && detail::tt_cutoff_allowed<Node>(record, tt_score, depth, alpha, beta)) {
            stats.main_tt_cutoff(search_ply);
            return tt_score;
        }

        tt_move = record.move;
    }

    /*
     * Static evaluation and early pruning.
     * Save the evaluation, then try non-PV pruning before generating moves.
     */
    const bool  in_check       = board.is_check();
    const Color side           = board.side_to_move();
    bool        quiet_futility = false;
    Score       static_eval    = TTRecord::no_static_eval;

    bool declining = false;
    if constexpr (Node == NodeType::NonPv) {
        const bool after_null = detail::after_null_move(board, search_ply);
        // Preserve the evaluation snapshot before trying early pruning.
        static_eval = tt_record && tt_record->has_static_eval() ? tt_record->static_eval
                                                                : eval::evaluate(board);
        if (!in_check) {
            frame.static_eval = static_eval;
            declining = search_ply >= 2 && frame.real_move && frames[search_ply - 1].real_move
                     && frames[search_ply - 2].static_eval
                     && static_eval < *frames[search_ply - 2].static_eval;
        }

        // At shallow depth, try quiescence when the evaluation is far below alpha.
        const bool razor_candidate = !excluded_search && !after_null && !in_check
                                  && depth <= detail::RazorMaxDepth && tt_move.is_null();
        if (razor_candidate && static_eval + detail::RazorMargin[depth] <= alpha) {
            stats.razor_try(search_ply);
            const Score value = quiescence<NodeType::NonPv>(alpha - 1, alpha);
            if (stop_requested())
                return alpha;
            if (value < alpha) {
                stats.razor_cutoff(search_ply);
                return value;
            }
        }

        // Skip reverse futility and null-move pruning when a deep TT bound is below beta.
        const bool tt_upper_veto = tt_record && tt_record->depth >= depth
                                && tt_record->bound == TTBound::UpperBound
                                && tt_record->score_at_ply(search_ply) < beta;

        // Reverse futility: return when the evaluation remains above beta after its margin.
        const bool reverse_futility_candidate =
            !excluded_search && !after_null && !in_check && depth <= detail::FutilityMaxDepth
            && beta > -scores::mate_bound && beta < scores::mate_bound;
        if (reverse_futility_candidate && board.non_pawn_material(side) > eval::piece(ROOK).mg
            && !tt_upper_veto && static_eval - detail::FutilityMargin[depth] >= beta)
            return static_eval;

        // Null-move pruning: try a reduced search after passing the turn.
        const bool null_move_candidate =
            !excluded_search && !after_null && !in_check && depth >= detail::NullMoveMinDepth;
        if (null_move_candidate && board.non_pawn_material(side) > eval::piece(ROOK).mg
            && !tt_upper_veto) {
            stats.null_move_try(search_ply);

            const int reduction = detail::null_move_reduction(depth, static_eval, beta);

            board.make_null(prefetch_child_tt);
            ++search_ply;
            const Score value =
                -alphabeta<NodeType::NonPv>(-beta, -beta + 1, depth - reduction, nullptr);
            // Restore the parent position before checking the result or stop request.
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
        quiet_futility = !excluded_search && depth <= detail::FutilityMaxDepth && !in_check
                      && alpha > -scores::mate_bound && alpha < scores::mate_bound
                      && static_eval + detail::FutilityMargin[depth] <= alpha;
    }

    /*
     * Singular search.
     * Search without the TT move to decide whether to extend it or take a multicut.
     */
    constexpr int SingularMinDepth     = 8;
    constexpr int SingularTtDepthSlack = 3;

    bool singular_extension = false;

    const bool within_singular_limits = depth >= SingularMinDepth && search_ply > 0
                                     && search_ply < 2 * root_iteration_depth
                                     && search_ply + 1 < engine::max_search_ply;

    // Require a sufficiently deep TT bound and room to extend this search path.
    if (tt_record && within_singular_limits && !excluded_search
        && !detail::after_null_move(board, search_ply)
        && tt_record->depth >= depth - SingularTtDepthSlack
        && (tt_record->bound == TTBound::Exact || tt_record->bound == TTBound::LowerBound)) {
        const Score tt_score       = tt_record->score_at_ply(search_ply);
        const Score singular_beta  = tt_score - std::max(1, depth * eval::pawn.mg / 50);
        const int   singular_depth = (depth - 1) / 2;

        const bool non_mate_scores = std::abs(tt_score) < scores::mate_bound
                                  && singular_beta > -scores::mate_bound
                                  && singular_beta < scores::mate_bound;
        if (non_mate_scores && !tt_move.is_null() && board.is_pseudo_legal(tt_move)
            && board.is_legal_pseudo_move(tt_move)) {
            // Search the same position without the TT move, then restore the parent state.
            const SearchFrame saved_frame = frame;
            const bool        was_active  = singular_search_active;
            frame.excluded_move           = tt_move;
            singular_search_active        = true;
            const Score singular_score    = alphabeta<NodeType::NonPv>(
                singular_beta - 1, singular_beta, singular_depth, nullptr);
            frame                  = saved_frame;
            singular_search_active = was_active;

            if (stop_requested())
                return alpha;

            if (singular_score < singular_beta) {
                singular_extension = true;
            } else if constexpr (Node == NodeType::NonPv) {
                const bool non_mate_window =
                    alpha > -scores::mate_bound && alpha < scores::mate_bound
                    && beta > -scores::mate_bound && beta < scores::mate_bound;

                // Multicuts also require the singular window's lower edge outside mate ranges.
                if (!singular_search_active && !in_check && non_mate_window
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

    const auto context = history.context_for(board, search_ply, tt_move);
    auto       moves   = movegen::MainSearchMoves(board, history, context);

    PrincipalVariation child_pv;
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
        const bool is_killer         = is_quiet && history.is_killer(move, search_ply);
        const bool is_counter        = is_quiet && move == context.counter_move;

        // LMR uses current history, read before making the move. Ordering retains its snapshot.
        int raw_capture_history = 0;
        if (is_capture && !is_promotion && !in_check)
            raw_capture_history = history.capture_score(board, move);
        bool history_lmp_candidate = false;
        if constexpr (Node == NodeType::NonPv) {
            const bool late_quiet_pruning_node =
                !excluded_search && depth == detail::LateMovePruningDepth
                && move_count > detail::LateMovePruningMoveCount && !quiet_futility && !in_check
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
        ++search_ply;

        // Child position: checking status gates pruning and reduction.
        const bool gives_check = board.is_check();
        if (quiet_futility && !first_legal && is_quiet && !gives_check) {
            // Futility pruning.
            board.unmake();
            --search_ply;
            moves.skip_nonchecking_quiets();
            stats.futility_skip(search_ply);
            continue;
        }

        // History-gated late-move pruning.
        if (history_lmp_candidate && !gives_check) {
            board.unmake();
            --search_ply;
            continue;
        }

        // Late-move reductions.
        const auto lmr       = detail::lmr_reduction<Node>({.depth           = depth,
                                                            .move_count      = move_count,
                                                            .is_quiet        = is_quiet,
                                                            .is_promotion    = is_promotion,
                                                            .in_check        = in_check,
                                                            .gives_check     = gives_check,
                                                            .is_killer       = is_killer,
                                                            .declining       = declining,
                                                            .is_tt_move      = move == tt_move,
                                                            .capture_history = raw_capture_history});
        const int  reduction = lmr.plies;
        stats.capture_lmr(lmr.capture_adjustment, raw_capture_history, lmr.base_plies, reduction);

        /*
         * Recursive search.
         * Scout reduced moves and later PV moves before a full-depth search.
         */
        const bool scout = reduction > 0 || (Node == NodeType::Pv && !first_legal);
        Score      value;
        if (scout) {
            if (reduction > 0)
                stats.lmr_try(search_ply - 1);
            value =
                -alphabeta<NodeType::NonPv>(-alpha - 1, -alpha, child_depth - reduction, nullptr);
        }

        if (!scout || (!stop_requested() && value > alpha)) {
            if (reduction > 0) {
                stats.lmr_research(search_ply - 1);
                stats.capture_research(lmr.capture_adjustment, lmr.base_plies != reduction);
            }
            if constexpr (Node == NodeType::Pv) {
                if (scout) {
                    stats.pvs_research(search_ply);
                    child_pv.clear();
                }
            }
            value = -alphabeta<Node>(
                -beta, -alpha, child_depth, Node == NodeType::Pv && pv ? &child_pv : nullptr);
        }

        /*
         * Move results.
         * Restore the parent position before stop checks, learning, PV updates, or TT stores.
         */
        board.unmake();
        --search_ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Train cutoff histories on the restored position.
            if (learning)
                learning->learn_from_cutoff(move, is_capture);

            if (pv)
                pv->update(move, child_pv);

            stats.beta_cutoff(search_ply, move_count);
            if (!excluded_search)
                tt.store(tt_probe.writer,
                         tt_key,
                         move,
                         value,
                         depth,
                         TTBound::LowerBound,
                         search_ply,
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
                if (pv)
                    pv->update(move, child_pv);
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
        best_value = in_check ? -scores::mate + search_ply : scores::draw;
        tt.store(tt_probe.writer,
                 tt_key,
                 NULL_MOVE,
                 best_value,
                 depth,
                 TTBound::Exact,
                 search_ply,
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
                 search_ply,
                 static_eval);

    return best_value;
}

// Quiescence search for tactical depth-zero nodes.
template <NodeType Node, bool UseTt>
Score Worker::quiescence(Score alpha, Score beta, PrincipalVariation* pv) {
    /*
     * PV reset and stop checks.
     * Clear the PV, check search limits, and return if a stop was requested.
     */
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    /*
     * Node counting and terminal exits.
     * Count this node, then return for a draw or the ply limit.
     */
    increment_nodes();
    stats.qnode(search_ply);

    if (board.is_draw(search_ply))
        return draw_or_mate_score(board, search_ply);

    if (search_ply >= engine::max_search_ply)
        return eval::evaluate(board);

    /*
     * TT lookup.
     * Check for a usable bound and retain the TT move for ordering.
     */
    constexpr int     qsearch_tt_depth = 0;
    const Score       original_alpha   = alpha;
    const PositionKey tt_key           = board.tt_key();
    Move              tt_move          = NULL_MOVE;
    TTProbe           tt_probe;
    const auto&       tt_record = tt_probe.record;

    if constexpr (UseTt) {
        stats.q_tt_probe(search_ply);
        tt_probe = tt.probe(tt_key);
        if (tt_record) {
            stats.q_tt_hit(search_ply);

            const Score tt_score = tt_record->score_at_ply(search_ply);
            if (detail::tt_cutoff_allowed<Node>(
                    *tt_record, tt_score, qsearch_tt_depth, alpha, beta)) {
                stats.q_tt_cutoff(search_ply);
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
                         qsearch_tt_depth,
                         TTBound::LowerBound,
                         search_ply,
                         static_eval);
            }
            return best_value;
        }
        if (best_value > alpha)
            alpha = best_value;
    }

    /*
     * Move preparation.
     * Set up tactical moves, or all legal evasions when in check.
     */
    auto               moves = movegen::QuiescenceMoves(board, history, tt_move);
    PrincipalVariation child_pv;

    for (Move move = moves.next(); !move.is_null(); move = moves.next()) {
        ++move_count;

        /*
         * Capture pruning.
         * Skip eligible nonchecking captures whose estimated gain cannot raise alpha.
         */
        // SEE prices classify exchanges; use evaluation material for the gain estimate.
        // This is a heuristic allowance for positional and phase changes, not a strict bound.
        const bool capture_pruning_candidate =
            !in_check && move != tt_move && move.type() == BASIC_MOVE && alpha > -scores::mate_bound
            && beta < scores::mate_bound;
        if (capture_pruning_candidate) {
            const PieceType victim = board.captured_piece_type(move);
            const auto      price  = eval::piece(victim);
            const Score     capture_gain_estimate =
                static_eval + std::max(price.mg, price.eg) + detail::QCaptureMargin;
            if (capture_gain_estimate < alpha) {
                const bool material_allows_capture_pruning =
                    board.non_pawn_material(board.side_to_move()) > eval::rook.mg
                    && board.non_pawn_material(~board.side_to_move())
                               - (victim == PAWN ? 0 : price.mg)
                           > eval::rook.mg;
                if (material_allows_capture_pruning && !board.gives_check(move)) {
                    // Keep a fail-low estimate without inventing an alpha raise or PV move.
                    best_value = std::max(best_value, capture_gain_estimate);
                    continue;
                }
            }
        }

        /*
         * Recursive search.
         * Make the move and search the child with the negated window.
         */
        board.make(move, UseTt ? prefetch_child_tt : nullptr);
        ++search_ply;
        const Score value = -quiescence<Node, UseTt>(-beta, -alpha, pv ? &child_pv : nullptr);

        /*
         * Move results.
         * Restore the parent position before stop checks, PV updates, or TT stores.
         */
        board.unmake();
        --search_ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            if (pv)
                pv->update(move, child_pv);
            stats.beta_cutoff(search_ply, move_count);
            if constexpr (UseTt) {
                tt.store(tt_probe.writer,
                         tt_key,
                         move,
                         value,
                         qsearch_tt_depth,
                         TTBound::LowerBound,
                         search_ply,
                         static_eval);
            }
            return value;
        }

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

    /*
     * Final node result.
     * Handle checkmate, otherwise store the best score and its bound.
     */
    if (in_check && move_count == 0) {
        best_value = -scores::mate + search_ply;
        if constexpr (UseTt) {
            tt.store(tt_probe.writer,
                     tt_key,
                     NULL_MOVE,
                     best_value,
                     qsearch_tt_depth,
                     TTBound::Exact,
                     search_ply,
                     static_eval);
        }
        return best_value;
    }

    if constexpr (UseTt) {
        tt.store(tt_probe.writer,
                 tt_key,
                 best_move,
                 best_value,
                 qsearch_tt_depth,
                 tt_bound_for_window(best_value, original_alpha, beta),
                 search_ply,
                 static_eval);
    }

    return best_value;
}

bool Worker::settle(Board& position) {
    configure_search(position, Limits{}, SearchClock::now());
    history.clear();

    PrincipalVariation pv;
    const Score        value = quiescence<NodeType::Pv, false>(-scores::inf, scores::inf, &pv);
    if (std::abs(value) >= scores::mate_bound)
        return false;

    for (int index = 0; index < pv.size(); ++index)
        board.make(pv.move_at(index));

    if (board.is_draw())
        return false;

    if (!movegen::UnscoredMoves(board).next().is_null()) {
        position = board;
        return true;
    }
    return false;
}

// Template definitions live in this translation unit; instantiate the node types we use.
template Score Worker::alphabeta<NodeType::Pv>(Score, Score, int, PrincipalVariation*);
template Score Worker::alphabeta<NodeType::NonPv>(Score, Score, int, PrincipalVariation*);
template Score Worker::quiescence<NodeType::Pv, true>(Score, Score, PrincipalVariation*);
template Score Worker::quiescence<NodeType::NonPv, true>(Score, Score, PrincipalVariation*);

} // namespace search
