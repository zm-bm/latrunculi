#include <algorithm>

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
constexpr int LmrMoves         = 2;
constexpr int LmrDepth         = 3;
constexpr int FutilityMargin   = 300;
constexpr int NullMoveR        = 3;

int Thread::search() {
    stats.reset();
    ply = 0;
    pv.clear();

    auto remaining      = board.sideToMove() == WHITE ? options.wtime : options.btime;
    auto increment      = board.sideToMove() == WHITE ? options.winc : options.binc;
    auto allocated      = remaining / options.movestogo + increment;
    this->allocatedTime = remaining > 0 ? std::min(allocated, options.movetime) : options.movetime;

    int score     = 0;
    int lastScore = eval<Silent>(board);
    int depth     = 1 + (threadId & 1);
    int lastDepth = depth - 1;

    // 1. Iterative deepening loop
    for (; depth <= options.depth && !stopSignal; ++depth) {
        stats.resetDebugStats();

        // 2. Aspiration window from previous score
        int alpha = lastScore - AspirationWindow;
        int beta  = lastScore + AspirationWindow;

        // 3. First search
        score = alphabeta(alpha, beta, depth);

        // 4. If fail-low or fail-high, re-search with bigger bounds
        if (score <= alpha) {
            score = alphabeta(-INF_SCORE, beta, depth);
        } else if (score >= beta) {
            score = alphabeta(alpha, INF_SCORE, depth);
        }

        if (score == ABORT_SCORE) break;
        lastScore = score;
        lastDepth = depth;

        heuristics.age();

        if (isMateScore(score)) break;
    }

    uciInfo(lastScore, lastDepth, true);
    uciBestMove();
    if (STATS_ENABLED) uciDebugStats();

    return score;
}

template <NodeType node>
int Thread::alphabeta(int alpha, int beta, int depth) {
    constexpr bool isRoot   = node == NodeType::Root;
    constexpr bool isPV     = isRoot || node == NodeType::PV;
    constexpr auto nodeType = isPV ? NodeType::PV : NodeType::NonPV;

    // Stop search when time expires
    if (stopSignal) {
        return ABORT_SCORE;
    } else if (isTimeUp()) {
        threadPool.stopAll();
    }

    // 1. Base case: quiescence search
    if (depth == 0) {
        return quiescence(alpha, beta);
    }

    U64 key        = board.getKey();
    int lowerbound = alpha;
    int upperbound = beta;
    int bestScore  = -INF_SCORE;
    Move bestMove  = NullMove;

    stats.addNode(ply);

    // 2. Check the transposition table
    stats.addTTProbe(ply);
    TT::Entry entry = TT::table.probe(key);
    if (entry.isValid(key) && entry.depth >= depth) {
        auto score = TT::score(entry.score, -ply);
        stats.addTTHit(ply);

        if constexpr (!isRoot) {
            if (entry.flag == TT::EXACT) {
                stats.addTTCutoff(ply);
                if (board.isLegalMove(entry.bestMove)) {
                    pv.update(ply, entry.bestMove);
                }
                return score;
            }
        }
        if constexpr (!isPV) {
            if (entry.flag == TT::LOWERBOUND && score >= beta) {
                stats.addTTCutoff(ply);
                return score;
            } else if (entry.flag == TT::UPPERBOUND && score <= alpha) {
                stats.addTTCutoff(ply);
                return score;
            }
        }
    }

    // Null move pruning
    bool inCheck = board.isCheck();
    if (!isPV && depth > NullMoveR && !inCheck) {
        board.makeNull();
        int score = -alphabeta<NodeType::NonPV>(-beta, -beta + 1, depth - 1 - NullMoveR);
        board.unmmakeNull();
        if (score == ABORT_SCORE) return ABORT_SCORE;
        if (score >= beta) return beta;
    }

    // 3. Generate moves and sort by priority
    MoveGenerator<GenType::All> moves{board};
    Move pvMove = pv.bestMove(ply);
    MoveOrder moveOrder(board,
                        heuristics,
                        ply,
                        (isPV ? pv.bestMove(ply) : NullMove),
                        entry.bestMove);
    moves.sort(moveOrder);

    // 4. Loop over moves
    int searchedMoves = 0;
    int legalMoves    = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        legalMoves++;

        bool isQuiet = !board.isCapture(move) && !board.isCheckingMove(move);

        // Futility pruning
        if (depth <= 2 && isQuiet) {
            int staticEval = eval(board);
            if (staticEval + (FutilityMargin * depth) <= alpha) continue;
        }

        // 5. Recursively search
        board.make(move);

        int score;
        pv.clear(ply + 1);
        if (isRoot || searchedMoves == 0) {
            score = -alphabeta<nodeType>(-beta, -alpha, depth - 1);
        } else {
            // late move reduction
            int LateMoveR = 0;
            if constexpr (!isPV) {
                if ((searchedMoves >= LmrMoves) && (depth >= LmrDepth) && isQuiet) {
                    LateMoveR = 1 + std::min(searchedMoves / 10, depth / 4);
                }
            }

            // null window search, re-search if fail-high in a PV node
            score = -alphabeta<NodeType::NonPV>(-alpha - 1, -alpha, depth - 1 - LateMoveR);
            if (score > alpha && isPV) {
                score = -alphabeta<NodeType::PV>(-beta, -alpha, depth - 1);
            }
        }
        searchedMoves++;

        board.unmake();

        if (score == ABORT_SCORE) return ABORT_SCORE;

        // 6. If we found a better move, update our bestScore/Move and principal variation
        if (score > bestScore) {
            bestScore = score;
            bestMove  = move;
            if (isPV && score > alpha) {
                pv.update(ply, move);
                if constexpr (isRoot) uciInfo(score, depth);
            }
        }

        // 7. Alpha-beta pruning
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            // Beta cut-off
            heuristics.addBetaCutoff(board, move, ply);
            stats.addBetaCutoff(ply, move == moves[0]);
            break;
        }
    }

    // Draw / mate handling
    if (legalMoves == 0) {
        bestMove = NullMove;
        if (board.isCheck()) {
            bestScore = -MATE_SCORE + ply;
        } else {
            bestScore = DRAW_SCORE;
        }
    }

    // 8. Store result in transposition table
    TT::NodeType flag = TT::EXACT;
    if (bestScore <= lowerbound)
        flag = TT::UPPERBOUND;
    else if (bestScore >= upperbound)
        flag = TT::LOWERBOUND;
    TT::table.store(key, bestMove, TT::score(bestScore, ply), depth, flag);

    if constexpr (isRoot) uciInfo(bestScore, depth);

    return bestScore;
}

template int Thread::alphabeta<NodeType::Root>(int, int, int);

int Thread::quiescence(int alpha, int beta) {
    if (stopSignal) {
        return ABORT_SCORE;
    } else if (isTimeUp()) {
        threadPool.stopAll();
    }

    // 1. Evaluate the current position (stand-pat).
    int standPat = eval(board);
    stats.addQNode(ply);

    if (standPat >= beta) {
        // beta cutoff
        return beta;
    }

    if (standPat > alpha) {
        // update alpha if position improves it
        alpha = standPat;
    }

    // 2. Generate only forcing moves and sort by priority
    MoveGenerator<GenType::Captures> moves{board};
    MoveOrder moveOrder(board, heuristics, ply);
    moves.sort(moveOrder);

    // 3. Loop over moves
    int legalMoves = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        legalMoves++;

        // Prune bad captures
        if (board.see(move) < 0) continue;

        // 4. Recursively search
        board.make(move);
        int score = -quiescence(-beta, -alpha);
        board.unmake();

        if (score == ABORT_SCORE) return ABORT_SCORE;

        if (score >= beta) {
            // beta cutoff
            stats.addBetaCutoff(ply, move == moves[0]);
            return beta;
        }
        if (score > alpha) {
            // update alpha
            alpha = score;
        }
    }

    if (legalMoves == 0) {
        if (board.isCheck())
            return -MATE_SCORE + ply;
        else if (board.isDraw())
            return 0;
    }

    return alpha;
}
