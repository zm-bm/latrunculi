#include <algorithm>

#include "base.hpp"
#include "board.hpp"
#include "constants.hpp"
#include "eval.hpp"
#include "move_order.hpp"
#include "movegen.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "tt.hpp"
#include "types.hpp"

constexpr int AspirationWindow = 50;

void Thread::reset() {
    nodes = 0;
    ply   = 0;

    killers.clear();
    history.clear();
    tt.ageTable();

    Color c    = board.sideToMove();
    searchTime = options.searchTimeMs(c);
}

int Thread::search() {
    reset();

    int value = eval(board);
    int depth = 1 + (threadId & 1);

    for (; depth <= options.depth; ++depth) {
        if constexpr (STATS_ENABLED) stats.reset();

        value = searchWiden(depth, value);

        if (stopSignal) break;

        rootValue = value;
        rootDepth = depth;

        uciHandler.info(getBestLine(value, depth));

        history.age();
    }

    if (threadId == 0) {
        if (stopSignal) {
            uciHandler.info(getBestLine(rootValue, rootDepth));
        }
        uciHandler.bestmove(rootMove.str());

        if constexpr (STATS_ENABLED) {
            auto stats = threadPool.accumulate(&Thread::stats);
            uciHandler.logOutput(stats);
        }
    }

    return rootValue;
}

int Thread::searchWiden(int depth, int prevValue) {
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
int Thread::alphabeta(int alpha, int beta, int depth, bool canNull) {
    constexpr bool root   = N == NodeType::Root;
    constexpr bool pvnode = N != NodeType::NonPV;
    constexpr auto child  = pvnode ? NodeType::PV : NodeType::NonPV;

    checkStop();
    if (stopSignal) return alpha;

    // mate distance pruning
    alpha = std::max(alpha, -MATE_VALUE + ply);
    beta  = std::min(beta, MATE_VALUE - ply);
    if (alpha >= beta) return alpha;

    // check extension
    bool inCheck = board.isCheck();
    if (inCheck) depth++;

    // base cases
    if (depth <= 0) return quiescence(alpha, beta);
    if (ply >= MAX_DEPTH) return eval(board);

    nodes++;
    stats.addNode(ply);

    // check for 50-move rule and draw by repetition
    if (board.isDraw()) return DRAW_VALUE;

    Color turn    = board.sideToMove();
    U64 key       = board.getKey();
    int alpha0    = alpha;
    int bestValue = -INF_VALUE;
    Move bestMove = NullMove;
    Move ttMove   = NullMove;
    Move pvMove   = root ? rootMove : NullMove;

    // Check the transposition table
    TT_Entry* e = tt.probe(key);
    stats.addTTProbe(ply);
    if (e != nullptr && board.isLegalMove(e->move)) {
        ttMove = e->move;

        if (e->depth >= depth) {
            stats.addTTHit(ply);
            int value = fromTT(e->score, ply);

            if constexpr (!pvnode) {
                if (e->flag == TT_Flag::Exact) {
                    stats.addTTCutoff(ply);
                    return value;
                }
            }

            if (e->flag == TT_Flag::LowerBound) alpha = std::max(alpha, value);
            if (e->flag == TT_Flag::UpperBound) beta = std::min(beta, value);

            if constexpr (!pvnode) {
                if (alpha >= beta) {
                    stats.addTTCutoff(ply);
                    return value;
                }
            }
        }
    }

    // Null move pruning
    if constexpr (!pvnode) {
        int R = 3;
        if (depth >= R && canNull && !inCheck && board.nonPawnMaterial(turn) > BISHOP_VALUE_MG) {
            if (depth > 6) R = 4;

            board.makeNull();
            int value = -alphabeta<NodeType::NonPV>(-beta, -beta + 1, depth - R, NO_NULL);
            board.unmmakeNull();

            if (stopSignal) return alpha;
            if (value >= beta) return value;
        }
    }

    // Static eval used in razoring and futility pruning
    int staticEval = 0;
    if constexpr (!pvnode) staticEval = eval(board);

    // Razoring: if eval is low, do a qsearch. if we can't beat alpha, fail low
    if constexpr (!pvnode) {
        constexpr int razorMargin[4] = {0, 500, 900, 1800};

        if (depth <= 3 && staticEval + razorMargin[depth] <= alpha) {
            int value = quiescence(alpha - 1, alpha);
            if (value < alpha) {
                return value;
            }
        }
    }

    // Decide if futile pruning is applicable
    bool futileNode = false;
    if constexpr (!pvnode) {
        constexpr int futilityMargin[4] = {0, 250, 400, 550};

        futileNode = (depth <= 3 && !inCheck && !isMate(alpha) &&
                      staticEval + futilityMargin[depth] <= alpha);
    }

    // Generate moves
    MoveGenerator<> moves{board};
    MoveOrder moveOrder(board, ply, killers, history, pvMove, ttMove);
    moves.sort(moveOrder);

    // Iterate through moves
    int legalMoves = 0;
    int triedMoves = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        legalMoves++;

        bool isPromotion = move.type() == MoveType::Promotion;
        bool isEnPassant = move.type() == MoveType::EnPassant;
        bool isCapture   = board.isCapture(move) || isEnPassant;
        bool isQuiet     = !isCapture && !isPromotion;

        board.make(move);

        bool givesCheck = board.isCheck();
        triedMoves++;

        // Futility pruning
        if (futileNode && triedMoves > 1 && isQuiet && !givesCheck) {
            board.unmake();
            continue;
        }

        // Late move reduction
        int reduction = 0;
        if constexpr (!root) {
            if (depth >= 3 && triedMoves >= 3) {
                double base = isQuiet ? 1.25 : 0.75;
                double div  = isQuiet ? 2.5 : 3.3;
                double r    = base + std::log(depth) * std::log(triedMoves) / div;

                if (inCheck || givesCheck) r *= 0.6;
                if constexpr (pvnode) r *= 0.7;
                if (killers.isKiller(move, ply)) r *= 0.8;

                reduction = std::clamp(int(r), 1, depth - 2);
            };
        }

        // PVS search
        int value;
        auto fullSearch = [&] { return -alphabeta<child>(-beta, -alpha, depth - 1); };

        if (triedMoves == 0) {
            value = fullSearch();
        } else {
            value = -alphabeta<NodeType::NonPV>(-alpha - 1, -alpha, depth - 1 - reduction);
            if constexpr (pvnode)
                if (value > alpha) value = fullSearch();
        }

        board.unmake();

        if (stopSignal) return alpha;

        // fail-soft beta cutoff
        if (value >= beta) {
            stats.addBetaCutoff(ply, move == moves[0]);

            if (isQuiet) {
                killers.update(move, ply);
                history.update(turn, move.from(), move.to(), depth);
            }

            tt.store(key, move, toTT(value, ply), depth, TT_Flag::LowerBound);

            return value;
        }

        // new best move
        if (value > bestValue) {
            bestValue = value;
            bestMove  = move;

            if (value > alpha) {
                alpha = value;
                if constexpr (root) {
                    rootValue = value;
                    rootDepth = depth;
                    rootMove  = move;
                }
            }
        }
    }

    if (legalMoves == 0) {
        bestMove  = NullMove;
        bestValue = inCheck ? -MATE_VALUE + ply : DRAW_VALUE;
    }

    TT_Flag flag = (bestValue > alpha0) ? TT_Flag::Exact : TT_Flag::UpperBound;
    tt.store(key, bestMove, toTT(bestValue, ply), depth, flag);

    return bestValue;
}

int Thread::quiescence(int alpha, int beta) {
    checkStop();
    if (stopSignal) return alpha;

    nodes++;
    stats.addQNode(ply);

    // check for 50-move rule and draw by repetition
    if (board.isDraw()) return DRAW_VALUE;

    int standPat = eval(board);
    if (ply >= MAX_DEPTH) return standPat;

    // mate distance pruning
    alpha = std::max(alpha, -MATE_VALUE + ply);
    beta  = std::min(beta, MATE_VALUE - ply);
    if (alpha >= beta) return alpha;

    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;

    MoveGenerator<MoveGenMode::Captures> moves{board};
    MoveOrder moveOrder(board, ply, killers, history);
    moves.sort(moveOrder);

    int legalMoves = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        legalMoves++;

        // Prune bad captures
        if (move.priority == 0) continue;

        board.make(move);
        int score = -quiescence(-beta, -alpha);
        board.unmake();

        if (stopSignal) return alpha;

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }

    if (legalMoves == 0) {
        if (board.isCheck()) return -MATE_VALUE + ply;
        if (board.isStalemate()) return DRAW_VALUE;
    }

    return alpha;
}
