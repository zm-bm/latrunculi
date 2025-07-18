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

void Thread::reset() {
    nodes = 0;
    ply   = 0;

    pvTable.clear();
    tt.ageTable();

    Color c    = board.sideToMove();
    searchTime = options.searchTimeMs(c);
}

int Thread::search() {
    reset();

    int depth = 1 + (threadId & 1);

    for (; depth <= options.depth; ++depth) {
        if constexpr (STATS_ENABLED) stats.reset();

        int value = alphabeta<>(-INF_SCORE, INF_SCORE, depth);

        if (stopSignal) break;
        sendInfo(value, depth, rootLine);
    }

    if (isMainThread()) {
        sendInfo(rootValue, rootDepth, rootLine);
        sendMove(rootLine.empty() ? NullMove : rootLine[0]);

        if constexpr (STATS_ENABLED) sendStats();
    }

    return rootValue;
}

template <NodeType N>
int Thread::alphabeta(int alpha, int beta, int depth) {
    constexpr bool root   = N == NodeType::Root;
    constexpr bool pvnode = N != NodeType::NonPV;
    constexpr auto child  = pvnode ? NodeType::PV : NodeType::NonPV;

    checkStop();
    if (stopSignal) return alpha;

    bool inCheck = board.isCheck();
    if (inCheck) depth++;

    if (depth == 0) return quiescence(alpha, beta);

    nodes++;
    stats.addNode(ply);

    // TODO: check for repetition or 50-move rule

    U64 key       = board.getKey();
    int alpha0    = alpha;
    int bestValue = -INF_SCORE;
    Move bestMove = NullMove;
    Move ttMove   = NullMove;

    TT_Entry* e = tt.probe(key);
    stats.addTTProbe(ply);
    if (e != nullptr && board.isLegalMove(e->move)) {
        ttMove = e->move;

        if (e->depth >= depth) {
            stats.addTTHit(ply);

            auto score = ttScore(e->score, -ply);

            if constexpr (!root) {
                if (e->flag == TT_Flag::Exact) {
                    stats.addTTCutoff(ply);
                    pvTable.update(ply, ttMove);
                    return score;
                }
            }

            if (e->flag == TT_Flag::LowerBound) alpha = std::max(alpha, score);
            if (e->flag == TT_Flag::UpperBound) beta = std::min(beta, score);

            if constexpr (!pvnode) {
                if (alpha >= beta) {
                    stats.addTTCutoff(ply);
                    pvTable.update(ply, ttMove);
                    return score;
                }
            }
        }
    }

    Move pvMove = pvTable.bestMove(ply);
    pvTable.clear(ply);

    MoveGenerator<> moves{board};
    MoveOrder moveOrder(board, heuristics, ply, pvMove, ttMove);
    moves.sort(moveOrder);

    int legalMoves = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        legalMoves++;

        board.make(move);

        auto fullSearch = [&] { return -alphabeta<child>(-beta, -alpha, depth - 1); };
        auto zwSearch = [&] { return -alphabeta<NodeType::NonPV>(-alpha - 1, -alpha, depth - 1); };
        int value;

        // PVS search
        if constexpr (root) {
            value = fullSearch();
        } else if (bestValue == -INF_SCORE) {
            value = fullSearch();
        } else {
            value = zwSearch();
            if constexpr (pvnode)
                if (value > alpha) value = fullSearch();
        }

        board.unmake();

        if (stopSignal) return alpha;

        if (value >= beta) {
            tt.store(key, move, ttScore(value, ply), depth, TT_Flag::LowerBound);
            pvTable.clear(ply);
            stats.addBetaCutoff(ply, move == moves[0]);
            return value;
        }

        if (value > bestValue) {
            bestValue = value;
            bestMove  = move;

            if (value > alpha) {
                alpha = value;
                if constexpr (pvnode) pvTable.update(ply, move);
                if constexpr (root) {
                    rootValue = value;
                    rootDepth = depth;
                    rootLine  = pvTable[0];
                    sendInfo(rootValue, rootDepth, rootLine);
                }
            }
        }
    }

    if (legalMoves == 0) {
        bestValue = inCheck ? -MATE_SCORE + ply : DRAW_SCORE;
    }

    TT_Flag flag = (bestValue > alpha0) ? TT_Flag::Exact : TT_Flag::UpperBound;
    tt.store(key, bestMove, ttScore(bestValue, ply), depth, flag);

    return bestValue;
}

int Thread::quiescence(int alpha, int beta) {
    checkStop();
    if (stopSignal) return alpha;

    nodes++;
    stats.addQNode(ply);

    int standPat = eval(board);

    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;

    MoveGenerator<MoveGenMode::Captures> moves{board};

    int legalMoves = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        legalMoves++;

        board.make(move);
        int score = -quiescence(-beta, -alpha);
        board.unmake();

        if (stopSignal) return alpha;

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }

    if (legalMoves == 0) {
        if (board.isCheck()) return -MATE_SCORE + ply;
        if (board.isDraw()) return DRAW_SCORE;
    }

    return alpha;
}

// -----------------------------
// Deprecated search functions
// -----------------------------

constexpr int AspirationWindow = 50;
constexpr int LmrMoves         = 2;
constexpr int LmrDepth         = 3;
constexpr int FutilityMargin   = 300;
constexpr int NullMoveR        = 3;

int Thread::search_DEPRECATED() {
    nodes = 0;
    ply   = 0;
    pvTable.clear();

    auto remaining   = board.sideToMove() == WHITE ? options.wtime : options.btime;
    auto increment   = board.sideToMove() == WHITE ? options.winc : options.binc;
    auto allocated   = remaining / options.movestogo + increment;
    this->searchTime = remaining > 0 ? std::min(allocated, options.movetime) : options.movetime;

    int score     = 0;
    int prevScore = eval(board);
    int depth     = 1 + (threadId & 1);
    int prevDepth = depth - 1;

    // 1. Iterative deepening loop
    for (; depth <= options.depth && !stopSignal; ++depth) {
        stats.reset();

        // 2. Aspiration window from previous score
        int alpha = prevScore - AspirationWindow;
        int beta  = prevScore + AspirationWindow;

        // 3. First search
        score = alphabeta<>(alpha, beta, depth);

        // 4. If fail-low or fail-high, re-search with bigger bounds
        if (score <= alpha) {
            score = alphabeta<>(-INF_SCORE, beta, depth);
        } else if (score >= beta) {
            score = alphabeta<>(alpha, INF_SCORE, depth);
        }

        if (score == ABORT_SCORE) break;
        prevScore = score;
        prevDepth = depth;

        heuristics.age();

        if (isMateScore(score)) break;
    }

    sendInfo(prevScore, prevDepth, pvTable[0]);
    if (isMainThread()) {
        uciHandler.bestmove(pvTable.bestMove().str());
        if constexpr (STATS_ENABLED) {
            uciHandler.logOutput(threadPool.accumulate(&Thread::stats));
        }
    }

    return score;
}

template <NodeType node>
int Thread::alphabeta_DEPRECATED(int alpha, int beta, int depth) {
    constexpr bool isRoot   = node == NodeType::Root;
    constexpr bool isPV     = isRoot || node == NodeType::PV;
    constexpr auto nodeType = isPV ? NodeType::PV : NodeType::NonPV;

    // Stop search when time expires
    if (stopSignal) {
        return ABORT_SCORE;
    } else if (isTimeUp_DEPRECATED()) {
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

    nodes++;
    stats.addNode(ply);

    // 2. Check the transposition tt
    stats.addTTProbe(ply);
    TT_Entry* entry = tt.probe(key);
    if (entry && entry->depth >= depth) {
        auto score = ttScore(entry->score, -ply);
        stats.addTTHit(ply);

        if constexpr (!isRoot) {
            if (entry->flag == TT_Flag::Exact) {
                stats.addTTCutoff(ply);
                if (board.isLegalMove(entry->move)) {
                    pvTable.update(ply, entry->move);
                }
                return score;
            }
        }
        if constexpr (!isPV) {
            if (entry->flag == TT_Flag::LowerBound && score >= beta) {
                stats.addTTCutoff(ply);
                return score;
            } else if (entry->flag == TT_Flag::UpperBound && score <= alpha) {
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
    MoveGenerator<MoveGenMode::All> moves{board};
    Move pvMove = pvTable.bestMove(ply);
    MoveOrder moveOrder(board,
                        heuristics,
                        ply,
                        (isPV ? pvTable.bestMove(ply) : NullMove),
                        (entry ? entry->move : NullMove));
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
        pvTable.clear(ply + 1);
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
                pvTable.update(ply, move);
                if constexpr (isRoot) sendInfo(score, depth, pvTable[0]);
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

    // 8. Store result in transposition tt
    TT_Flag flag = TT_Flag::Exact;
    if (bestScore <= lowerbound)
        flag = TT_Flag::UpperBound;
    else if (bestScore >= upperbound)
        flag = TT_Flag::LowerBound;
    tt.store(key, bestMove, ttScore(bestScore, ply), depth, flag);

    if constexpr (isRoot) sendInfo(bestScore, depth, pvTable[0]);

    return bestScore;
}

template int Thread::alphabeta_DEPRECATED<NodeType::Root>(int, int, int);

int Thread::quiescence_DEPRECATED(int alpha, int beta) {
    if (stopSignal) {
        return ABORT_SCORE;
    } else if (isTimeUp_DEPRECATED()) {
        threadPool.stopAll();
    }

    // 1. Evaluate the current position (stand-pat).
    int standPat = eval(board);

    nodes++;
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
    MoveGenerator<MoveGenMode::Captures> moves{board};
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
