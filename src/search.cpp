#include <algorithm>

#include "board.hpp"

#include "defs.hpp"
#include "evaluator.hpp"
#include "movegen.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "tt.hpp"

constexpr int AspirationWindow = 50;

void Thread::reset() {
    nodes = 0;
    ply   = 0;

    killers.clear();
    history.clear();
    tt.age_table();
}

int Thread::search() {
    reset();

    int value = evaluate(board);
    int depth = 1 + (thread_id & 1);

    for (; depth <= options.depth; ++depth) {
        if constexpr (SEARCH_STATS)
            stats.reset();

        value = search_widen(depth, value);

        if (stop_signal)
            break;

        root_value = value;
        root_depth = depth;

        protocol.info(get_pv_line(value, depth));

        history.age();
    }

    if (thread_id == 0) {
        if (stop_signal) {
            protocol.info(get_pv_line(root_value, root_depth));
        }
        protocol.bestmove(root_move.str());

        if constexpr (SEARCH_STATS) {
            auto stats = thread_pool.accumulate(&Thread::stats);
            protocol.diagnostic_output(stats);
        }
    }

    return root_value;
}

int Thread::search_widen(int depth, int prevValue) {
    int alpha = prevValue - AspirationWindow;
    int beta  = prevValue + AspirationWindow;

    int value = alphabeta<>(alpha, beta, depth);

    if (value <= alpha)
        value = alphabeta<>(-INF_VALUE, beta, depth);
    else if (value >= beta)
        value = alphabeta<>(alpha, INF_VALUE, depth);

    return value;
}

template <NodeType N>
int Thread::alphabeta(int alpha, int beta, int depth, bool can_null) {
    constexpr bool root   = N == ROOT;
    constexpr bool pvnode = N != NON_PV;
    constexpr auto child  = pvnode ? PV : NON_PV;

    check_stop();
    if (stop_signal)
        return alpha;

    // mate distance pruning
    alpha = std::max(alpha, -MATE_VALUE + ply);
    beta  = std::min(beta, MATE_VALUE - ply);
    if (alpha >= beta)
        return alpha;

    // check extension
    bool in_check = board.is_check();
    if (in_check)
        depth++;

    // base cases
    if (depth <= 0)
        return quiescence(alpha, beta);
    if (ply >= MAX_DEPTH)
        return evaluate(board);

    nodes++;
    stats.node(ply);

    // check for 50-move rule and draw by repetition
    if (board.is_draw())
        return DRAW_VALUE;

    Color    turn       = board.side_to_move();
    uint64_t key        = board.key();
    int      alpha0     = alpha;
    int      best_value = -INF_VALUE;
    Move     best_move  = NULL_MOVE;
    Move     tt_move    = NULL_MOVE;
    Move     pv_move    = root ? root_move : NULL_MOVE;

    // Probe the transposition table
    TT_Entry* e = tt.probe(key);
    stats.tt_probe(ply);

    // If we have an entry, check for cutoffs or update alpha/beta
    if (e != nullptr && board.is_legal_move(e->move)) {
        tt_move = e->move;

        if (e->depth >= depth) {
            stats.tt_hit(ply);
            int value = e->get_score(ply);

            if constexpr (!pvnode) {
                if (e->flag == TT_Flag::Exact) {
                    stats.tt_cutoff(ply);
                    return value;
                }
            }

            if (e->flag == TT_Flag::Lowerbound)
                alpha = std::max(alpha, value);
            if (e->flag == TT_Flag::Upperbound)
                beta = std::min(beta, value);

            if constexpr (!pvnode) {
                if (alpha >= beta) {
                    stats.tt_cutoff(ply);
                    return value;
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

            board.make_null();
            int value = -alphabeta<NON_PV>(-beta, -beta + 1, depth - R, false);
            board.unmake_null();

            if (stop_signal)
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
            int value = quiescence(alpha - 1, alpha);
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
    auto movelist = generate<ALL_MOVES>(board);
    movelist.sort({board, killers, history, ply, pv_move, tt_move});

    // Iterate through moves
    int legal_moves = 0;
    int tried_moves = 0;
    for (auto& move : movelist) {
        if (!board.is_legal_move(move))
            continue;
        legal_moves++;

        bool is_promotion = move.type() == MOVE_PROM;
        bool is_enpassant = move.type() == MOVE_EP;
        bool is_capture   = board.is_capture(move) || is_enpassant;
        bool is_quiet     = !is_capture && !is_promotion;

        board.make(move);

        bool gives_check = board.is_check();
        tried_moves++;

        // Futility pruning
        if (futile_flag && tried_moves > 1 && is_quiet && !gives_check) {
            board.unmake();
            continue;
        }

        // Late move reduction
        int reduction = 0;
        if constexpr (!root) {
            if (depth >= 3 && tried_moves >= 3) {
                double base = is_quiet ? 1.25 : 0.75;
                double div  = is_quiet ? 2.5 : 3.3;
                double r    = base + std::log(depth) * std::log(tried_moves) / div;

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
        if (tried_moves == 0) {
            value = -alphabeta<child>(-beta, -alpha, depth - 1, can_null);
        } else {
            value = -alphabeta<NON_PV>(-alpha - 1, -alpha, depth - 1 - reduction, can_null);
            if constexpr (pvnode) {
                if (value > alpha)
                    value = -alphabeta<child>(-beta, -alpha, depth - 1, can_null);
            }
        }

        board.unmake();

        if (stop_signal)
            return alpha;

        // fail-soft beta cutoff
        if (value >= beta) {
            if (is_quiet) {
                killers.update(move, ply);
                history.update(turn, move.from(), move.to(), depth);
            }

            stats.beta_cutoff(ply, move == movelist[0]);
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

    if (legal_moves == 0) {
        best_move  = NULL_MOVE;
        best_value = in_check ? -MATE_VALUE + ply : DRAW_VALUE;
    }

    TT_Flag flag = (best_value > alpha0) ? TT_Flag::Exact : TT_Flag::Upperbound;
    tt.store(key, best_move, best_value, depth, flag, ply);

    return best_value;
}

int Thread::quiescence(int alpha, int beta) {
    check_stop();
    if (stop_signal)
        return alpha;

    nodes++;
    stats.qnode(ply);

    // check for 50-move rule and draw by repetition
    if (board.is_draw())
        return DRAW_VALUE;

    int stand_pat = evaluate(board);
    if (ply >= MAX_DEPTH)
        return stand_pat;

    // mate distance pruning
    alpha = std::max(alpha, -MATE_VALUE + ply);
    beta  = std::min(beta, MATE_VALUE - ply);
    if (alpha >= beta)
        return alpha;

    if (stand_pat >= beta)
        return stand_pat;
    if (stand_pat > alpha)
        alpha = stand_pat;

    auto movelist = generate<CAPTURES>(board);
    movelist.sort({board, killers, history, ply});

    int legal_moves = 0;
    for (auto& move : movelist) {
        if (!board.is_legal_move(move))
            continue;
        legal_moves++;

        // Prune bad captures
        if (move.priority == 0)
            continue;

        board.make(move);
        int score = -quiescence(-beta, -alpha);
        board.unmake();

        if (stop_signal)
            return alpha;

        if (score >= beta)
            return score;
        if (score > alpha)
            alpha = score;
    }

    if (legal_moves == 0) {
        if (board.is_check())
            return -MATE_VALUE + ply;
        if (board.is_stalemate())
            return DRAW_VALUE;
    }

    return alpha;
}
