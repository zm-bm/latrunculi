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
EvalValue draw_or_mate_score(const Board& board, int search_ply) {
    if (board.halfmove_clock() >= 100 && board.is_check()) {
        return movegen::UnscoredMoves(board).next().is_null() ? -eval_value::mate + search_ply
                                                              : eval_value::draw;
    }
    return eval_value::draw;
}

} // namespace

// Recursive main search: alpha-beta for non-PV nodes, PVS for PV nodes.
template <NodeType Node>
EvalValue Worker::alphabeta(EvalValue alpha, EvalValue beta, int depth, PrincipalVariation* pv) {
    // PV and stop checks.
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    // Draw, max-ply, and qsearch exits.
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

    auto&      frame             = frames[search_ply];
    const Move excluded_move     = frame.excluded_move;
    const bool verification_root = !excluded_move.is_null();
    assert(!verification_root || depth > 0);
    frame.static_eval.reset();
    frame.real_move = !board.previous_move().is_null();
    if (depth <= 0)
        return quiescence<Node>(alpha, beta, pv);

    increment_nodes();
    stats.node(search_ply);

    // Mate-distance pruning.
    alpha = std::max(alpha, -eval_value::mate + search_ply);
    beta  = std::min(beta, eval_value::mate - search_ply - 1);
    if (alpha >= beta)
        return alpha;

    const EvalValue   original_alpha = alpha;
    const PositionKey tt_key         = board.tt_key();
    Move              tt_move        = NULL_MOVE;

    // TT probe.
    stats.main_tt_probe(search_ply);
    const auto  tt_probe  = tt.probe(tt_key);
    const auto& tt_record = tt_probe.record;
    if (tt_record) {
        stats.main_tt_hit(search_ply);

        const TTRecord& record   = *tt_record;
        const EvalValue tt_score = record.score_at_ply(search_ply);
        if (!verification_root
            && detail::tt_cutoff_allowed<Node>(record, tt_score, depth, alpha, beta)) {
            stats.main_tt_cutoff(search_ply);
            return tt_score;
        }

        tt_move = record.move;
    }

    const bool  in_check    = board.is_check();
    const Color side        = board.side_to_move();
    bool        futility    = false;
    EvalValue   static_eval = TTRecord::no_static_eval;

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
        if (!verification_root && !after_null && !in_check && depth <= detail::RazorMaxDepth
            && tt_move.is_null() && static_eval + detail::RazorMargin[depth] <= alpha) {
            stats.razor_try(search_ply);
            const EvalValue value = quiescence<NodeType::NonPv>(alpha - 1, alpha);
            if (stop_requested())
                return alpha;
            if (value < alpha) {
                stats.razor_cutoff(search_ply);
                return value;
            }
        }

        // Reverse futility and null-move pruning.
        // Skip static high-side cutoffs and NMP when a depth-sufficient TT upper bound suggests
        // they will fail low.
        const bool tt_upper_veto = tt_record && tt_record->depth >= depth
                                && tt_record->bound == TTBound::UpperBound
                                && tt_record->score_at_ply(search_ply) < beta;
        if (!verification_root && !after_null && !in_check && depth <= detail::FutilityMaxDepth
            && beta > -eval_value::mate_bound && beta < eval_value::mate_bound
            && board.non_pawn_material(side) > eval::piece(ROOK).mg && !tt_upper_veto
            && static_eval - detail::FutilityMargin[depth] >= beta)
            return static_eval;

        if (!verification_root && !after_null && !in_check && depth >= detail::NullMoveMinDepth
            && board.non_pawn_material(side) > eval::piece(ROOK).mg && !tt_upper_veto) {
            stats.null_move_try(search_ply);

            const int reduction = detail::null_move_reduction(depth, static_eval, beta);

            board.make_null(prefetch_child_tt);
            ++search_ply;
            const EvalValue value =
                -alphabeta<NodeType::NonPv>(-beta, -beta + 1, depth - reduction, nullptr);
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
        futility = !verification_root && depth <= detail::FutilityMaxDepth && !in_check
                && alpha > -eval_value::mate_bound && alpha < eval_value::mate_bound
                && static_eval + detail::FutilityMargin[depth] <= alpha;
    }

    // Same-ply verification restores its frame before interpreting the result.
    bool                     singular_extension = false;
    std::optional<EvalValue> singular_cutoff;
    if (tt_record) {
        if (const auto verification = detail::singular_verification<Node>(board,
                                                                          *tt_record,
                                                                          frame,
                                                                          search_ply,
                                                                          root_iteration_depth,
                                                                          depth,
                                                                          alpha,
                                                                          beta,
                                                                          verification_active)) {
            EvalValue value;
            {
                const detail::ScopedSingularVerification scope{
                    frame, verification_active, tt_record->move};
                value = alphabeta<NodeType::NonPv>(
                    verification->beta - 1, verification->beta, verification->depth, nullptr);
            }
            if (!stop_requested()) {
                if (value < verification->beta)
                    singular_extension = true;
                else if (verification->multicut_allowed)
                    singular_cutoff = verification->beta;
            }
        }
    }
    if (stop_requested())
        return alpha;
    // A multicut returns before the ordinary move stream, TT store, or cutoff training.
    if (singular_cutoff)
        return *singular_cutoff;

    // Move ordering and node-local history feedback.
    int       move_count = 0;
    EvalValue best_value = -eval_value::inf;
    Move      best_move  = NULL_MOVE;

    const auto context = history.context_for(board, search_ply, tt_move);
    auto       moves   = movegen::MainSearchMoves(board, history, context);

    PrincipalVariation child_pv;
    // Verification roots neither train history nor contribute learning instrumentation.
    std::optional<HistoryLearning> learning;
    if (!verification_root)
        learning.emplace(board, history, context, depth, stats);

    // Move loop.
    for (Move move = moves.next(); !move.is_null(); move = moves.next()) {
        if (move == excluded_move)
            continue;

        ++move_count;
        const bool first_legal = move_count == 1;

        // Parent position: classify the move and read current learning tables.
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
            history_lmp_candidate = !verification_root && depth == detail::LateMovePruningDepth
                                 && move_count > detail::LateMovePruningMoveCount && !futility
                                 && !in_check && best_value > -eval_value::mate_bound
                                 && alpha > -eval_value::mate_bound && beta < eval_value::mate_bound
                                 && board.non_pawn_material(side) > 0 && is_ordinary_quiet
                                 && move != tt_move && !is_killer && !is_counter
                                 && history.quiet_score(context, board, move) < 0;
        }

        const int child_depth = depth - 1 + (move == tt_move && singular_extension);
        board.make(move, prefetch_child_tt);
        ++search_ply;

        // Child position: checking status gates pruning and reduction.
        const bool gives_check = board.is_check();
        if (futility && !first_legal && is_quiet && !gives_check) {
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

        // Scout reduced moves and later PV moves before a full-depth search.
        const bool scout = reduction > 0 || (Node == NodeType::Pv && !first_legal);
        EvalValue  value;
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

        // Restore before stop handling, learning, PV updates, or TT stores.
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
            if (!verification_root)
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
        // Best-move update.
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

    // Mate and stalemate.
    if (move_count == 0) {
        // Excluding the only move does not make the actual position terminal.
        if (verification_root)
            return original_alpha;
        best_value = in_check ? -eval_value::mate + search_ply : eval_value::draw;
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

    // TT store.
    if (!verification_root)
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
EvalValue Worker::quiescence(EvalValue alpha, EvalValue beta, PrincipalVariation* pv) {
    // PV and stop checks.
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    increment_nodes();
    stats.qnode(search_ply);

    // Draw and max-ply exits.
    if (board.is_draw(search_ply))
        return draw_or_mate_score(board, search_ply);

    if (search_ply >= engine::max_search_ply)
        return eval::evaluate(board);

    constexpr int     qsearch_tt_depth = 0;
    const EvalValue   original_alpha   = alpha;
    const PositionKey tt_key           = board.tt_key();
    Move              tt_move          = NULL_MOVE;
    TTProbe           tt_probe;
    const auto&       tt_record = tt_probe.record;

    // TT probe.
    if constexpr (UseTt) {
        stats.q_tt_probe(search_ply);
        tt_probe = tt.probe(tt_key);
        if (tt_record) {
            stats.q_tt_hit(search_ply);

            const EvalValue tt_score = tt_record->score_at_ply(search_ply);
            if (detail::tt_cutoff_allowed<Node>(
                    *tt_record, tt_score, qsearch_tt_depth, alpha, beta)) {
                stats.q_tt_cutoff(search_ply);
                return tt_score;
            }

            tt_move = tt_record->move;
        }
    }

    const bool in_check    = board.is_check();
    int        move_count  = 0;
    EvalValue  best_value  = -eval_value::inf;
    Move       best_move   = NULL_MOVE;
    EvalValue  static_eval = TTRecord::no_static_eval;

    // Stand pat.
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

    auto               moves = movegen::QuiescenceMoves(board, history, tt_move);
    PrincipalVariation child_pv;

    // Tactical move or evasion loop.
    for (Move move = moves.next(); !move.is_null(); move = moves.next()) {
        ++move_count;

        // SEE prices classify exchanges; use evaluation material for the gain estimate.
        // This is a heuristic allowance for positional and phase changes, not a strict bound.
        if (!in_check && move != tt_move && move.type() == BASIC_MOVE
            && alpha > -eval_value::mate_bound && beta < eval_value::mate_bound) {
            const PieceType victim = board.captured_piece_type(move);
            const auto      price  = eval::piece(victim);
            const EvalValue gain_bound =
                static_eval + std::max(price.mg, price.eg) + detail::QCaptureMargin;
            if (gain_bound < alpha && board.non_pawn_material(board.side_to_move()) > eval::rook.mg
                && board.non_pawn_material(~board.side_to_move()) - (victim == PAWN ? 0 : price.mg)
                       > eval::rook.mg
                && !board.gives_check(move)) {
                // Keep a fail-low estimate without inventing an alpha raise or PV move.
                best_value = std::max(best_value, gain_bound);
                continue;
            }
        }

        board.make(move, UseTt ? prefetch_child_tt : nullptr);
        ++search_ply;
        const EvalValue value = -quiescence<Node, UseTt>(-beta, -alpha, pv ? &child_pv : nullptr);
        // Restore before stop handling, PV updates, or TT stores.
        board.unmake();
        --search_ply;

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Beta cutoff.
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

        // Best-move update.
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

    // Checkmate.
    if (in_check && move_count == 0) {
        best_value = -eval_value::mate + search_ply;
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

    // TT store.
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
    const EvalValue value = quiescence<NodeType::Pv, false>(-eval_value::inf, eval_value::inf, &pv);
    if (std::abs(value) >= eval_value::mate_bound)
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
template EvalValue Worker::alphabeta<NodeType::Pv>(EvalValue, EvalValue, int, PrincipalVariation*);
template EvalValue
Worker::alphabeta<NodeType::NonPv>(EvalValue, EvalValue, int, PrincipalVariation*);
template EvalValue
Worker::quiescence<NodeType::Pv, true>(EvalValue, EvalValue, PrincipalVariation*);
template EvalValue
Worker::quiescence<NodeType::NonPv, true>(EvalValue, EvalValue, PrincipalVariation*);

} // namespace search
