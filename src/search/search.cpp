#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>

#include "core/constants.hpp"
#include "eval/evaluation.hpp"
#include "movegen/moves.hpp"
#include "search/history_learning.hpp"
#include "search/search_detail.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"

namespace search {

namespace {

// Called after Board::is_draw(). Checkmate takes precedence over the fifty-move rule.
EvalValue draw_or_mate_score(const Board& board, int ply) {
    if (board.halfmove_clock() >= 100 && board.is_check()) {
        return movegen::UnscoredMoves(board).next().is_null() ? -eval_value::mate + ply
                                                              : eval_value::draw;
    }
    return eval_value::draw;
}

} // namespace

// Recursive main search: alpha-beta for non-PV nodes, PVS for PV nodes.
template <NodeType Node>
EvalValue
Worker::alphabeta(EvalValue alpha, EvalValue beta, int depth, int ply, PrincipalVariation* pv) {
    // Step 1. PV and stop checks.
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    // Step 2. Draw, max-ply, and qsearch exits.
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

    auto&      frame             = frames[ply];
    const Move excluded_move     = frame.excluded_move;
    const bool verification_root = !excluded_move.is_null();
    assert(!verification_root || depth > 0);
    frame.static_eval.reset();
    frame.real_move = !board.previous_move().is_null();
    if (depth <= 0)
        return quiescence<Node>(alpha, beta, ply, pv);

    increment_nodes();
    stats.node(ply);

    // Step 3. Mate-distance pruning.
    alpha = std::max(alpha, -eval_value::mate + ply);
    beta  = std::min(beta, eval_value::mate - ply - 1);
    if (alpha >= beta)
        return alpha;

    const EvalValue   original_alpha = alpha;
    const PositionKey tt_key         = board.tt_key();
    Move              tt_move        = NULL_MOVE;

    // Step 4. TT probe.
    stats.main_tt_probe(ply);
    const auto  tt_probe  = tt.probe(tt_key);
    const auto& tt_record = tt_probe.record;
    if (tt_record) {
        stats.main_tt_hit(ply);

        const TTRecord& record   = *tt_record;
        const EvalValue tt_score = record.score_at_ply(ply);
        if (!verification_root
            && detail::tt_cutoff_allowed<Node>(record, tt_score, depth, alpha, beta)) {
            stats.main_tt_cutoff(ply);
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
        const bool after_null = after_null_move(ply);
        // Step 5. Razoring.
        static_eval = tt_record && tt_record->has_static_eval() ? tt_record->static_eval
                                                                : eval::evaluate(board);
        if (!in_check) {
            frame.static_eval = static_eval;
            declining         = ply >= 2 && frame.real_move && frames[ply - 1].real_move
                     && frames[ply - 2].static_eval && static_eval < *frames[ply - 2].static_eval;
        }
        if (!verification_root && !after_null && !in_check && depth <= detail::RazorMaxDepth
            && tt_move.is_null() && static_eval + detail::RazorMargin[depth] <= alpha) {
            stats.razor_try(ply);
            const EvalValue value = quiescence<NodeType::NonPv>(alpha - 1, alpha, ply);
            if (stop_requested())
                return alpha;
            if (value < alpha) {
                stats.razor_cutoff(ply);
                return value;
            }
        }

        // Step 6. Reverse futility and null-move pruning.
        // Skip static high-side cutoffs and NMP when a depth-sufficient TT upper bound suggests
        // they will fail low.
        const bool tt_upper_veto = tt_record && tt_record->depth >= depth
                                && tt_record->bound == TTBound::UpperBound
                                && tt_record->score_at_ply(ply) < beta;
        if (!verification_root && !after_null && !in_check && depth <= detail::FutilityMaxDepth
            && beta > -eval_value::mate_bound && beta < eval_value::mate_bound
            && board.non_pawn_material(side) > eval::piece(ROOK).mg && !tt_upper_veto
            && static_eval - detail::FutilityMargin[depth] >= beta)
            return static_eval;

        if (!verification_root && !after_null && !in_check && depth >= detail::NullMoveMinDepth
            && board.non_pawn_material(side) > eval::piece(ROOK).mg && !tt_upper_veto) {
            stats.null_move_try(ply);

            const int reduction = detail::null_move_reduction(depth, static_eval, beta);

            board.make_null(detail::prefetch_child_tt);
            const EvalValue value =
                -alphabeta<NodeType::NonPv>(-beta, -beta + 1, depth - reduction, ply + 1, nullptr);
            board.unmake_null();

            if (stop_requested())
                return alpha;
            if (value >= beta) {
                stats.null_move_cutoff(ply);
                return value;
            }
        }

        // Prepare shallow futility pruning. The move loop performs the actual skip.
        futility = !verification_root && depth <= detail::FutilityMaxDepth && !in_check
                && alpha > -eval_value::mate_bound && alpha < eval_value::mate_bound
                && static_eval + detail::FutilityMargin[depth] <= alpha;
    }

    SingularResult singular;
    if (tt_record)
        singular = singular_decision<Node>(*tt_record, depth, ply, alpha, beta);
    if (stop_requested())
        return alpha;
    // A multicut returns before the ordinary move stream, TT store, or cutoff training.
    if (singular.cutoff)
        return *singular.cutoff;

    // Step 7. Move ordering and node-local history feedback.
    int       move_count = 0;
    EvalValue best_value = -eval_value::inf;
    Move      best_move  = NULL_MOVE;

    const auto context = history.context_for(board, ply, tt_move);
    auto       moves   = movegen::MainSearchMoves(board, history, context);

    PrincipalVariation child_pv;
    // Verification roots neither train history nor contribute learning instrumentation.
    std::optional<HistoryLearning> learning;
    if (!verification_root)
        learning.emplace(board, history, context, depth, stats);

    // Step 8. Move loop.
    for (Move move = moves.next(); !move.is_null(); move = moves.next()) {
        if (move == excluded_move)
            continue;

        ++move_count;
        const bool first_legal = move_count == 1;

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
            history_lmp_candidate = !verification_root && depth == detail::LateMovePruningDepth
                                 && move_count > detail::LateMovePruningMoveCount && !futility
                                 && !in_check && best_value > -eval_value::mate_bound
                                 && alpha > -eval_value::mate_bound && beta < eval_value::mate_bound
                                 && board.non_pawn_material(side) > 0 && is_ordinary_quiet
                                 && move != tt_move && !is_killer && !is_counter
                                 && history.quiet_score(context, board, move) < 0;
        }

        const int child_depth = depth - 1 + (move == tt_move && singular.extend);
        board.make(move, detail::prefetch_child_tt);

        const bool gives_check = board.is_check();
        if (futility && !first_legal && is_quiet && !gives_check) {
            // Step 9. Futility pruning.
            board.unmake();
            moves.skip_nonchecking_quiets();
            stats.futility_skip(ply);
            continue;
        }

        // Step 10. History-gated late-move pruning.
        if (history_lmp_candidate && !gives_check) {
            board.unmake();
            continue;
        }

        // Step 11. Late-move reductions.
        const auto base_reduction = detail::lmr_reduction<Node>(
            depth, move_count, is_quiet, is_promotion, in_check, gives_check, is_killer, declining);
        int        reduction            = base_reduction.plies;
        const bool capture_lmr_eligible = detail::capture_lmr_eligible({
            .non_pv         = Node == NodeType::NonPv,
            .is_capture     = is_capture,
            .is_tt_move     = move == tt_move,
            .depth          = depth,
            .base_reduction = reduction,
            .is_promotion   = is_promotion,
            .in_check       = in_check,
            .gives_check    = gives_check,
        });
        if (capture_lmr_eligible)
            reduction =
                detail::capture_lmr_reduction(base_reduction.unrounded, raw_capture_history, depth);
        stats.capture_lmr(
            capture_lmr_eligible, raw_capture_history, base_reduction.plies, reduction);

        // Step 12. Scout reduced moves and later PV moves before a full-depth search.
        const bool scout = reduction > 0 || (Node == NodeType::Pv && !first_legal);
        EvalValue  value;
        if (scout) {
            if (reduction > 0)
                stats.lmr_try(ply);
            value = -alphabeta<NodeType::NonPv>(
                -alpha - 1, -alpha, child_depth - reduction, ply + 1, nullptr);
        }

        if (!scout || (!stop_requested() && value > alpha)) {
            if (reduction > 0) {
                stats.lmr_research(ply);
                stats.capture_research(capture_lmr_eligible, base_reduction.plies != reduction);
            }
            if constexpr (Node == NodeType::Pv) {
                if (scout) {
                    stats.pvs_research(ply + 1);
                    child_pv.clear();
                }
            }
            value = -alphabeta<Node>(-beta,
                                     -alpha,
                                     child_depth,
                                     ply + 1,
                                     Node == NodeType::Pv && pv ? &child_pv : nullptr);
        }

        board.unmake();

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Step 13. Train cutoff histories on the restored position.
            if (learning)
                learning->learn_from_cutoff(move, is_capture);

            if (pv)
                pv->update(move, child_pv);

            stats.beta_cutoff(ply, move_count);
            if (!verification_root)
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
        // Step 14. Best-move update.
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

    // Step 15. Mate and stalemate.
    if (move_count == 0) {
        // Excluding the only move does not make the actual position terminal.
        if (verification_root)
            return original_alpha;
        best_value = in_check ? -eval_value::mate + ply : eval_value::draw;
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

    // Step 16. TT store.
    if (!verification_root)
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
template <NodeType Node, bool UseTt>
EvalValue Worker::quiescence(EvalValue alpha, EvalValue beta, int ply, PrincipalVariation* pv) {
    // Step 1. PV and stop checks.
    if (pv)
        pv->clear();

    if (should_poll_search_limits())
        poll_search_limits();
    if (stop_requested())
        return alpha;

    increment_nodes();
    stats.qnode(ply);

    // Step 2. Draw and max-ply exits.
    if (board.is_draw(ply))
        return draw_or_mate_score(board, ply);

    if (ply >= engine::max_search_ply)
        return eval::evaluate(board);

    constexpr int     qsearch_tt_depth = 0;
    const EvalValue   original_alpha   = alpha;
    const PositionKey tt_key           = board.tt_key();
    Move              tt_move          = NULL_MOVE;
    TTProbe           tt_probe;
    const auto&       tt_record = tt_probe.record;

    // Step 3. TT probe.
    if constexpr (UseTt) {
        stats.q_tt_probe(ply);
        tt_probe = tt.probe(tt_key);
        if (tt_record) {
            stats.q_tt_hit(ply);

            const EvalValue tt_score = tt_record->score_at_ply(ply);
            if (detail::tt_cutoff_allowed<Node>(
                    *tt_record, tt_score, qsearch_tt_depth, alpha, beta)) {
                stats.q_tt_cutoff(ply);
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

    // Step 4. Stand pat.
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
                         ply,
                         static_eval);
            }
            return best_value;
        }
        if (best_value > alpha)
            alpha = best_value;
    }

    auto               moves = movegen::QuiescenceMoves(board, history, tt_move);
    PrincipalVariation child_pv;

    // Step 5. Tactical move or evasion loop.
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

        board.make(move, UseTt ? detail::prefetch_child_tt : nullptr);
        const EvalValue value =
            -quiescence<Node, UseTt>(-beta, -alpha, ply + 1, pv ? &child_pv : nullptr);
        board.unmake();

        if (stop_requested())
            return alpha;

        if (value >= beta) {
            // Step 6. Beta cutoff.
            if (pv)
                pv->update(move, child_pv);
            stats.beta_cutoff(ply, move_count);
            if constexpr (UseTt) {
                tt.store(tt_probe.writer,
                         tt_key,
                         move,
                         value,
                         qsearch_tt_depth,
                         TTBound::LowerBound,
                         ply,
                         static_eval);
            }
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
        best_value = -eval_value::mate + ply;
        if constexpr (UseTt) {
            tt.store(tt_probe.writer,
                     tt_key,
                     NULL_MOVE,
                     best_value,
                     qsearch_tt_depth,
                     TTBound::Exact,
                     ply,
                     static_eval);
        }
        return best_value;
    }

    // Step 9. TT store.
    if constexpr (UseTt) {
        tt.store(tt_probe.writer,
                 tt_key,
                 best_move,
                 best_value,
                 qsearch_tt_depth,
                 tt_bound_for_window(best_value, original_alpha, beta),
                 ply,
                 static_eval);
    }

    return best_value;
}

bool Worker::settle(Board& position) {
    configure_search(position, Limits{}, SearchClock::now());
    history.clear();

    PrincipalVariation pv;
    const EvalValue    value =
        quiescence<NodeType::Pv, false>(-eval_value::inf, eval_value::inf, 0, &pv);
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
template EvalValue
Worker::alphabeta<NodeType::Pv>(EvalValue, EvalValue, int, int, PrincipalVariation*);
template EvalValue
Worker::alphabeta<NodeType::NonPv>(EvalValue, EvalValue, int, int, PrincipalVariation*);
template EvalValue
Worker::quiescence<NodeType::Pv, true>(EvalValue, EvalValue, int, PrincipalVariation*);
template EvalValue
Worker::quiescence<NodeType::NonPv, true>(EvalValue, EvalValue, int, PrincipalVariation*);

} // namespace search
