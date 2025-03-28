#include "search.hpp"

#include <algorithm>

#include "board.hpp"
#include "eval.hpp"
#include "movegen.hpp"
#include "thread.hpp"
#include "tt.hpp"

namespace Search {

constexpr int FullDepthMoves = 4;
constexpr int ReductionLimit = 3;
constexpr int FutilityMargin = 300;
constexpr int NullMoveR      = 4;

template <NodeType node>
int search(Thread& thread, int alpha, int beta, int depth) {
    constexpr bool isRoot   = node == NodeType::Root;
    constexpr bool isPV     = isRoot || node == NodeType::PV;
    constexpr auto nodeType = isPV ? NodeType::PV : NodeType::NonPV;

    // 1. Base case: quiescence search
    if (depth == 0) {
        return quiescence(thread, alpha, beta);
    }

    Board& board   = thread.board;
    U64 key        = board.getKey();
    int lowerbound = alpha;
    int upperbound = beta;
    int bestScore  = -INF_VALUE;
    Move bestMove;

    thread.stats.addNode(thread.ply);

    // 2. Check the transposition table
    TT::Entry* entry = nullptr;
    if constexpr (!isPV) {
        thread.stats.addTTProbe(thread.ply);
        entry = TT::table.probe(key);

        if (entry->isValid(key) && entry->depth >= depth) {
            thread.stats.addTTHit(thread.ply);
            int score = ttScore(entry->score, -thread.ply);

            if (entry->flag == TT::EXACT) {
                thread.pv.update(thread.ply, entry->bestMove);
                thread.stats.addTTCutoff(thread.ply);
                return score;
            }
            if (entry->flag == TT::LOWERBOUND && score >= beta) {
                thread.stats.addTTCutoff(thread.ply);
                return score;
            }
            if (entry->flag == TT::UPPERBOUND && score <= alpha) {
                thread.stats.addTTCutoff(thread.ply);
                return score;
            }
        }
    }

    // Null move pruning
    if (!isPV && depth >= NullMoveR && !board.isCheck()) {
        board.makeNull();
        int score = -search<NodeType::NonPV>(thread, -beta, -beta + 1, depth - NullMoveR);
        board.unmmakeNull();
        if (score >= beta) return beta;
    }

    // 3. Generate moves and sort by priority
    MoveGenerator<GenType::All> moves{board};
    moves.sort(MovePriority(thread, isPV, entry));

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
        if (isRoot || searchedMoves == 0) {
            score = -search<nodeType>(thread, -beta, -alpha, depth - 1);
        } else {
            // late move reduction
            bool doReduce = (searchedMoves >= FullDepthMoves) && (depth >= ReductionLimit);
            int reduction =
                (doReduce && !isPV && isQuiet) ? 1 + std::min(searchedMoves / 10, depth / 4) : 0;

            // null window search, re-search if fail-high in a PV node
            score = -search<NodeType::NonPV>(thread, -alpha - 1, -alpha, depth - 1 - reduction);
            if (score > alpha && isPV) {
                score = -search<NodeType::PV>(thread, -beta, -alpha, depth - 1);
            }
        }
        searchedMoves++;

        board.unmake();

        // 6. If we found a better move, update our bestScore/Move and principal variation
        if (score > bestScore) {
            bestScore = score;
            bestMove  = move;
            thread.pv.update(thread.ply, move);
        }

        // 7. Alpha-beta pruning
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            // Beta cut-off, update heuristics if quiet move
            thread.heuristics.updateBetaCutoff(board, move, thread.ply);
            thread.stats.addBetaCutoff(thread.ply, move == moves[0]);
            break;
        }
    }

    // Draw / mate handling
    if (legalMoves == 0) {
        if (board.isCheck()) {
            bestScore = -MATE_VALUE + thread.ply;
            bestMove  = Move();
        } else {
            bestScore = 0;
            bestMove  = Move();
        }
    }

    // 8. Store result in transposition table
    TT::NodeType flag = TT::EXACT;
    if (bestScore <= lowerbound)
        flag = TT::UPPERBOUND;
    else if (bestScore >= upperbound)
        flag = TT::LOWERBOUND;
    TT::table.store(key, bestMove, ttScore(bestScore, thread.ply), depth, flag);

    return bestScore;
}

template int search<NodeType::Root>(Thread&, int, int, int);

int quiescence(Thread& thread, int alpha, int beta) {
    // 1. Evaluate the current position (stand-pat).
    Board& board = thread.board;
    int standPat = eval(board);
    thread.stats.addQNode(thread.ply);

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
    moves.sort(MovePriority(thread));

    // 3. Loop over moves
    int legalMoves = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        legalMoves++;

        // Prune bad captures
        if (board.see(move) < 0) continue;

        // 4. Recursively search
        board.make(move);
        int score = -quiescence(thread, -beta, -alpha);
        board.unmake();

        if (score >= beta) {
            // beta cutoff
            thread.stats.addBetaCutoff(thread.ply, move == moves[0]);
            return beta;
        }
        if (score > alpha) {
            // update alpha
            alpha = score;
        }
    }

    if (legalMoves == 0) {
        if (board.isCheck())
            return -MATE_VALUE + thread.ply;
        else if (board.isDraw())
            return 0;
    }

    return alpha;
}

template <NodeType node>
U64 perft(int depth, Board& board, std::ostream& oss) {
    if (depth == 0) return 1;

    MoveGenerator<GenType::All> moves{board};

    U64 count = 0, nodes = 0;

    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;

        board.make(move);

        count  = perft<NodeType::NonPV>(depth - 1, board);
        nodes += count;

        if constexpr (node == NodeType::Root) {
            oss << move << ": " << count << '\n';
        }

        board.unmake();
    }

    if constexpr (node == NodeType::Root) {
        oss << "NODES: " << nodes << std::endl;
    }

    return nodes;
}

template U64 perft<NodeType::Root>(int, Board&, std::ostream& = std::cout);

}  // namespace Search
