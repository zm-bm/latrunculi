#include <algorithm>

#include "board.hpp"
#include "eval.hpp"
#include "movegen.hpp"
#include "thread.hpp"
#include "tt.hpp"
#include "uci.hpp"

constexpr int AspirationWindow = 33;
constexpr int FullDepthMoves   = 4;
constexpr int ReductionLimit   = 3;
constexpr int FutilityMargin   = 300;
constexpr int NullMoveR        = 4;

int Thread::search() {
    reset();

    int score     = 0;
    int prevScore = 0;

    // 1. Iterative deepening loop
    for (int depth = 1; depth <= options.depth && !ThreadPool::stopSignal; ++depth) {
        stats.resetDepthStats();

        // 2. Aspiration window from previous score
        int alpha = prevScore - AspirationWindow;
        int beta  = prevScore + AspirationWindow;

        // 3. First search
        score = alphabeta(alpha, beta, depth);

        // 4. If fail-low or fail-high, re-search with bigger bounds
        if (score <= alpha) {
            score = alphabeta(-INF_SCORE, beta, depth);
        } else if (score >= beta) {
            score = alphabeta(alpha, INF_SCORE, depth);
        }

        prevScore = score;
        heuristics.age();
        UCI::printInfo(output, score, depth, stats, pv);

        if (isMateScore(score)) break;
    }

    output << "bestmove " << pv.bestMove() << std::endl;
    if (options.debug) UCI::printDebuggingInfo(output, stats);

    return score;
}

template <NodeType node>
int Thread::alphabeta(int alpha, int beta, int depth) {
    constexpr bool isRoot   = node == NodeType::Root;
    constexpr bool isPV     = isRoot || node == NodeType::PV;
    constexpr auto nodeType = isPV ? NodeType::PV : NodeType::NonPV;

    // Stop search when time expires
    if (stats.checkTime(options.movetime)) ThreadPool::stopSignal = true;

    // 1. Base case: quiescence search
    if (depth == 0) {
        return quiescence(alpha, beta);
    }

    U64 key        = board.getKey();
    int lowerbound = alpha;
    int upperbound = beta;
    int bestScore  = -INF_SCORE;
    Move bestMove;

    stats.addNode(ply);

    // 2. Check the transposition table
    TT::Entry* entry = nullptr;
    if constexpr (!isPV) {
        stats.addTTProbe(ply);
        entry = TT::table.probe(key);

        if (entry->isValid(key) && entry->depth >= depth) {
            stats.addTTHit(ply);
            auto score    = TT::score(entry->score, -ply);
            auto bestMove = entry->bestMove;
            auto flag     = entry->flag;

            TT::table.release(key);

            if (entry->flag == TT::EXACT) {
                pv.update(ply, bestMove);
                stats.addTTCutoff(ply);
                return score;
            }
            if (entry->flag == TT::LOWERBOUND && score >= beta) {
                stats.addTTCutoff(ply);
                return score;
            }
            if (entry->flag == TT::UPPERBOUND && score <= alpha) {
                stats.addTTCutoff(ply);
                return score;
            }
        } else {
            TT::table.release(key);
        }
    }

    // Null move pruning
    if (!isPV && depth >= NullMoveR && !board.isCheck()) {
        board.makeNull();
        int score = -alphabeta<NodeType::NonPV>(-beta, -beta + 1, depth - NullMoveR);
        board.unmmakeNull();
        if (score >= beta) return beta;
    }

    // 3. Generate moves and sort by priority
    MoveGenerator<GenType::All> moves{board};
    moves.sort(MovePriority(*this, isPV, entry));

    // 4. Loop over moves
    int searchedMoves = 0;
    int legalMoves    = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        legalMoves++;
        bool isQuiet = !board.isCapture(move) && !board.isCheckingMove(move);

        // Return best estimate when search stops
        if (ThreadPool::stopSignal) {
            if (bestScore > -INF_SCORE) return bestScore;
            return eval(board);
        }

        // Futility pruning
        if (depth <= 2 && isQuiet) {
            int staticEval = eval(board);
            if (staticEval + (FutilityMargin * depth) <= alpha) continue;
        }

        // 5. Recursively search
        board.make(move);

        int score;
        if (isRoot || searchedMoves == 0) {
            score = -alphabeta<nodeType>(-beta, -alpha, depth - 1);
        } else {
            // late move reduction
            bool doReduce = (searchedMoves >= FullDepthMoves) && (depth >= ReductionLimit);
            int reduction =
                (doReduce && !isPV && isQuiet) ? 1 + std::min(searchedMoves / 10, depth / 4) : 0;

            // null window search, re-search if fail-high in a PV node
            score = -alphabeta<NodeType::NonPV>(-alpha - 1, -alpha, depth - 1 - reduction);
            if (score > alpha && isPV) {
                score = -alphabeta<NodeType::PV>(-beta, -alpha, depth - 1);
            }
        }
        searchedMoves++;

        board.unmake();

        // 6. If we found a better move, update our bestScore/Move and principal variation
        if (score > bestScore) {
            bestScore = score;
            bestMove  = move;
            pv.update(ply, move);
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
        if (board.isCheck()) {
            bestScore = -MATE_SCORE + ply;
            bestMove  = Move();
        } else {
            bestScore = DRAW_SCORE;
            bestMove  = Move();
        }
    }

    // 8. Store result in transposition table
    TT::NodeType flag = TT::EXACT;
    if (bestScore <= lowerbound)
        flag = TT::UPPERBOUND;
    else if (bestScore >= upperbound)
        flag = TT::LOWERBOUND;
    TT::table.store(key, bestMove, TT::score(bestScore, ply), depth, flag);

    return bestScore;
}

template int Thread::alphabeta<NodeType::Root>(int, int, int);

int Thread::quiescence(int alpha, int beta) {
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
    moves.sort(MovePriority(*this));

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
