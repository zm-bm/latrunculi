#include <algorithm>
#include <cmath>

#include "board.hpp"

#include "defs.hpp"
#include "evaluator.hpp"
#include "movegen.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "tt.hpp"

namespace {
constexpr int AspirationWindow = 50;
constexpr int QSearchTTDepth   = 0;

struct SearchWindow {
    int  alpha;
    int  beta;
    int  cutoff;
    bool has_cutoff;
};

SearchWindow apply_mate_distance_pruning(int alpha, int beta, int ply) {
    const int lower_bound = -MATE_VALUE + ply;
    if (lower_bound >= beta)
        return {alpha, beta, lower_bound, true};
    alpha = std::max(alpha, lower_bound);

    const int upper_bound = MATE_VALUE - ply - 1;
    if (upper_bound <= alpha)
        return {alpha, beta, upper_bound, true};
    beta = std::min(beta, upper_bound);

    return {alpha, beta, 0, false};
}
} // namespace

void Thread::reset() {
    nodes      = 0;
    ply        = 0;
    root_depth = 0;
    root_value = evaluate(board);
    root_move  = first_legal_root_move();

    killers.clear();
    history.clear();
}

int Thread::initial_search_depth() const {
    return 1 + (thread_id & 1);
}

int Thread::search() {
    reset();

    int value = root_value;
    int depth = initial_search_depth();

    for (; depth <= options.depth; ++depth) {
        if constexpr (SEARCH_STATS)
            stats.reset();

        value = search_widen(depth, value);

        if (halt_requested())
            break;

        root_value = value;
        root_depth = depth;

        if (thread_id == 0)
            protocol.info(get_pv_line(value, depth));

        history.age();
    }

    publish_root_result();

    if (thread_id == 0) {
        thread_pool.halt_helpers();
        thread_pool.wait_helpers();

        if (halt_requested()) {
            protocol.info(get_pv_line(root_value, root_depth));
        }

        Move best_move = thread_pool.best_voted_move(board, root_move);
        protocol.bestmove(best_move.str());

        if constexpr (SEARCH_STATS) {
            auto stats = thread_pool.accumulate(&Thread::stats);
            protocol.diagnostic_output(stats);
        }
    }

    return root_value;
}

int Thread::search_widen(int depth, int prevValue) {
    constexpr int Inf    = static_cast<int>(INF_VALUE);
    constexpr int NegInf = -Inf;

    int delta = AspirationWindow;
    int alpha = std::max(prevValue - delta, NegInf);
    int beta  = std::min(prevValue + delta, Inf);

    while (true) {
        int value = alphabeta<>(alpha, beta, depth);

        if (halt_requested())
            return value;

        if (value <= alpha) {
            stats.aspiration_fail_low();
            if (alpha == NegInf)
                return value;
            alpha = std::max(alpha - delta, NegInf);
            stats.aspiration_research();
        } else if (value >= beta) {
            stats.aspiration_fail_high();
            if (beta == Inf)
                return value;
            beta = std::min(beta + delta, Inf);
            stats.aspiration_research();
        } else {
            return value;
        }

        delta = std::min(delta * 2, Inf);
    }
}

template <NodeType N>
int Thread::alphabeta(int alpha, int beta, int depth, bool can_null) {
    constexpr bool root   = N == ROOT;
    constexpr bool pvnode = N != NON_PV;
    constexpr auto child  = pvnode ? PV : NON_PV;

    check_halt_conditions();
    if (halt_requested())
        return alpha;

    uint64_t key = board.key();
    __builtin_prefetch(tt.prefetch_addr(key));

    // mate distance pruning
    auto window = apply_mate_distance_pruning(alpha, beta, ply);
    if (window.has_cutoff)
        return window.cutoff;
    alpha = window.alpha;
    beta  = window.beta;

    // check extension
    bool in_check = board.is_check();
    if (in_check)
        depth++;

    // base cases
    if (depth <= 0)
        return quiescence<child>(alpha, beta);
    if (ply >= MAX_SEARCH_PLY)
        return evaluate(board);

    nodes++;
    stats.node(ply);

    // check for 50-move rule and draw by repetition
    if (board.is_draw(ply))
        return DRAW_VALUE;

    Color turn       = board.side_to_move();
    int   alpha0     = alpha;
    int   best_value = -INF_VALUE;
    Move  best_move  = NULL_MOVE;
    Move  tt_move    = NULL_MOVE;
    Move  pv_move    = (root && root_depth > 0) ? root_move : NULL_MOVE;

    // Probe the transposition table via a local snapshot only.
    auto probe = tt.probe(key);
    stats.main_tt_probe(ply);

    // If we have an entry, check for cutoffs.
    // The search must consume only the snapshot returned by probe().
    if (probe.has_value()) {
        tt_move = probe->move;

        if constexpr (!pvnode) {
            if (probe->depth >= depth) {
                stats.main_tt_hit(ply);
                int value = probe->get_score(ply);

                if (probe->flag == TT_Flag::Exact) {
                    stats.main_tt_cutoff(ply);
                    return value;
                }

                if (probe->flag == TT_Flag::Lowerbound) {
                    if (value >= beta) {
                        stats.main_tt_cutoff(ply);
                        return value;
                    }
                }

                if (probe->flag == TT_Flag::Upperbound) {
                    if (value <= alpha) {
                        stats.main_tt_cutoff(ply);
                        return value;
                    }
                }
            }
        }
    }

    // Null move pruning
    if constexpr (!pvnode) {
        int R = 3;
        if (depth >= R && can_null && !in_check && board.nonPawnMaterial(turn) > BISHOP_MG) {
            if (depth > 6)
                R = 4;

            board.make_null(position_states[ply + 1]);
            ++ply;
            int value = -alphabeta<NON_PV>(-beta, -beta + 1, depth - R, false);
            board.unmake_null(position_states[ply - 1]);
            --ply;

            if (halt_requested())
                return alpha;
            if (value >= beta)
                return value;
        }
    }

    // Static eval used in razoring and futility pruning
    int static_eval = 0;
    if constexpr (!pvnode)
        static_eval = evaluate(board);

    // Razoring: if eval is low, do a qsearch. if we can't beat alpha, fail low
    if constexpr (!pvnode) {
        constexpr int razor_margin[4] = {0, 500, 900, 1800};

        if (depth <= 3 && static_eval + razor_margin[depth] <= alpha) {
            int value = quiescence<NON_PV>(alpha - 1, alpha);
            if (value < alpha) {
                return value;
            }
        }
    }

    // Decide if futile pruning is applicable
    bool futile_flag = false;
    if constexpr (!pvnode) {
        constexpr int futility_margin[4] = {0, 250, 400, 550};
        futile_flag = (depth <= 3 && !in_check && std::abs(alpha) < MATE_BOUND &&
                       static_eval + futility_margin[depth] <= alpha);
    }

    // Generate moves
    auto             movelist = generate<ALL_MOVES>(board);
    StagedMovePicker picker{{board, killers, history, ply, pv_move, tt_move, root, thread_id},
                            movelist};

    // Iterate through moves
    int legal_move_index = 0;
    while (Move* picked_move = picker.next()) {
        Move move = *picked_move;
        if (!board.is_legal_pseudo_move(move))
            continue;
        legal_move_index++;
        const bool first_legal = (legal_move_index == 1);

        bool is_promotion = move.type() == MOVE_PROM;
        bool is_enpassant = move.type() == MOVE_EP;
        bool is_capture   = board.is_capture(move) || is_enpassant;
        bool is_quiet     = !is_capture && !is_promotion;

        board.make(move, position_states[ply + 1]);
        ++ply;

        bool gives_check = board.is_check();

        // Futility pruning
        if (futile_flag && !first_legal && is_quiet && !gives_check) {
            board.unmake(position_states[ply - 1]);
            --ply;
            continue;
        }

        // Late move reduction
        int reduction = 0;
        if constexpr (!root) {
            if (depth >= 3 && legal_move_index >= 3) {
                double base = is_quiet ? 1.25 : 0.75;
                double div  = is_quiet ? 2.5 : 3.3;
                double r    = base + std::log(depth) * std::log(legal_move_index) / div;

                if (in_check || gives_check)
                    r *= 0.6;
                if constexpr (pvnode)
                    r *= 0.7;
                if (killers.is_killer(move, ply))
                    r *= 0.8;

                reduction = std::clamp(int(r), 1, depth - 2);
            };
        }

        // PVS search
        int value;
        if (first_legal) {
            value = -alphabeta<child>(-beta, -alpha, depth - 1);
        } else {
            value = -alphabeta<NON_PV>(-alpha - 1, -alpha, depth - 1 - reduction);
            if constexpr (pvnode) {
                if (value > alpha) {
                    stats.pvs_research(ply);
                    value = -alphabeta<child>(-beta, -alpha, depth - 1);
                }
            }
        }

        board.unmake(position_states[ply - 1]);
        --ply;

        if (halt_requested()) {
            if constexpr (root)
                return root_value;
            return alpha;
        }

        // fail-soft beta cutoff
        if (value >= beta) {
            if (is_quiet) {
                killers.update(move, ply);
                history.update(turn, move.from(), move.to(), depth);
            }

            if constexpr (root) {
                root_move  = move;
                root_value = value;
                root_depth = depth;
            }

            stats.beta_cutoff(ply, legal_move_index);
            tt.store(key, move, value, depth, TT_Flag::Lowerbound, ply);
            return value;
        }

        // new best move
        if (value > best_value) {
            best_value = value;
            best_move  = move;

            if (value > alpha) {
                alpha = value;
                if constexpr (root) {
                    root_value = value;
                    root_depth = depth;
                    root_move  = move;
                }
            }
        }
    }

    if (legal_move_index == 0) {
        best_move  = NULL_MOVE;
        best_value = in_check ? -MATE_VALUE + ply : DRAW_VALUE;
    }

    if constexpr (root) {
        root_move  = best_move;
        root_value = best_value;
        root_depth = depth;
    }

    TT_Flag flag = (best_value > alpha0) ? TT_Flag::Exact : TT_Flag::Upperbound;
    tt.store(key, best_move, best_value, depth, flag, ply);

    return best_value;
}

template <NodeType N>
int Thread::quiescence(int alpha, int beta, int qply) {
    constexpr bool pvnode = N != NON_PV;

    check_halt_conditions();
    if (halt_requested())
        return alpha;

    nodes++;
    stats.qnode(ply);

    // mate distance pruning
    auto window = apply_mate_distance_pruning(alpha, beta, ply);
    if (window.has_cutoff)
        return window.cutoff;
    alpha = window.alpha;
    beta  = window.beta;

    if (ply >= MAX_SEARCH_PLY)
        return evaluate(board);

    // check for 50-move rule and draw by repetition
    if (board.is_draw(ply))
        return DRAW_VALUE;

    const uint64_t key       = board.key();
    const int      alpha0    = alpha;
    const bool     qtt_root  = !pvnode && qply == 0;
    auto           store_qtt = [&](int score, TT_Flag flag) {
        if (qtt_root)
            tt.store(key, NULL_MOVE, score, QSearchTTDepth, flag, ply);
    };

    if (qtt_root) {
        auto probe = tt.probe(key);
        stats.q_tt_probe(ply);
        if (probe.has_value() && probe->depth >= QSearchTTDepth) {
            stats.q_tt_hit(ply);

            if constexpr (!pvnode) {
                const int value = probe->get_score(ply);
                if (probe->flag == TT_Flag::Exact) {
                    stats.q_tt_cutoff(ply);
                    return value;
                }
                if (probe->flag == TT_Flag::Lowerbound && value >= beta) {
                    stats.q_tt_cutoff(ply);
                    return value;
                }
                if (probe->flag == TT_Flag::Upperbound && value <= alpha) {
                    stats.q_tt_cutoff(ply);
                    return value;
                }
            }
        }
    }

    const bool in_check   = board.is_check();
    int        best_value = -INF_VALUE;

    if (!in_check) {
        int stand_pat = evaluate(board);
        best_value    = stand_pat;
        if (stand_pat >= beta) {
            if (board.is_stalemate()) {
                store_qtt(DRAW_VALUE, TT_Flag::Exact);
                return DRAW_VALUE;
            }
            store_qtt(stand_pat, TT_Flag::Lowerbound);
            return stand_pat;
        }
        if (stand_pat > alpha)
            alpha = stand_pat;
    }

    MoveList movelist = in_check ? generate<EVASIONS>(board) : generate<CAPTURES>(board);
    QuiescenceMovePicker picker{{board, killers, history, ply}, movelist, in_check};

    int legal_moves = 0;
    while (Move* picked_move = picker.next()) {
        Move move = *picked_move;
        if (!board.is_legal_pseudo_move(move))
            continue;
        legal_moves++;

        // Prune bad captures only in the normal non-check qsearch branch.
        if (!in_check && move.type() != MOVE_PROM && move.priority == PRIORITY_WEAK)
            continue;

        board.make(move, position_states[ply + 1]);
        ++ply;
        int score = -quiescence<N>(-beta, -alpha, qply + 1);
        board.unmake(position_states[ply - 1]);
        --ply;

        if (halt_requested())
            return alpha;

        if (score >= beta) {
            store_qtt(score, TT_Flag::Lowerbound);
            return score;
        }
        if (score > best_value) {
            best_value = score;
            if (score > alpha)
                alpha = score;
        }
    }

    if (legal_moves == 0) {
        if (in_check) {
            best_value = -MATE_VALUE + ply;
            store_qtt(best_value, TT_Flag::Exact);
            return best_value;
        }
        if (board.is_stalemate()) {
            store_qtt(DRAW_VALUE, TT_Flag::Exact);
            return DRAW_VALUE;
        }
    }

    const TT_Flag flag = (best_value > alpha0) ? TT_Flag::Exact : TT_Flag::Upperbound;
    store_qtt(best_value, flag);
    return best_value;
}

template int Thread::quiescence<NON_PV>(int alpha, int beta, int qply);
template int Thread::quiescence<PV>(int alpha, int beta, int qply);
