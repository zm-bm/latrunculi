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
    constexpr bool isRoot = node == NodeType::Root;
    constexpr bool isPV   = node == NodeType::Root || node == NodeType::PV;

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

    thread.stats.totalNodes++;
    if (thread.options.debug) thread.stats.nodes[thread.depth]++;

    // 2. Check the transposition table
    TT::Entry* entry;
    if constexpr (!isPV) {
        if (thread.options.debug) thread.stats.ttProbes[thread.depth]++;
        entry = TT::table.probe(key);
        if (entry->isValid(key) && entry->depth >= depth) {
            if (thread.options.debug) thread.stats.ttHits[thread.depth]++;

            int score = entry->score;
            if (score >= MATE_IN_MAX_PLY) score -= thread.depth;
            if (score <= -MATE_IN_MAX_PLY) score += thread.depth;

            if (entry->flag == TT::EXACT) {
                thread.pv.update(entry->bestMove, thread.depth);
                if (thread.options.debug) thread.stats.ttCutoffs[thread.depth]++;
                return score;
            }
            if (entry->flag == TT::LOWERBOUND && score >= beta) {
                if (thread.options.debug) thread.stats.ttCutoffs[thread.depth]++;
                return score;
            }
            if (entry->flag == TT::UPPERBOUND && score <= alpha) {
                if (thread.options.debug) thread.stats.ttCutoffs[thread.depth]++;
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
    moves.sort(MovePriority(thread, node, entry));

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
        bool doFullSearch = isRoot || (isPV && searchedMoves == 0);
        if (doFullSearch) {
            score = -search<NodeType::PV>(thread, -beta, -alpha, depth - 1);
        } else {
            // LMR + null window search, re-search if it fail-high
            bool doReduce = (searchedMoves >= FullDepthMoves) && (depth >= ReductionLimit);
            int reduction =
                (doReduce && !isPV && isQuiet) ? 1 + std::min(searchedMoves / 10, depth / 4) : 0;

            score = -search<NodeType::NonPV>(thread, -alpha - 1, -alpha, depth - 1 - reduction);
            if (score > alpha) {
                score = -search<node>(thread, -beta, -alpha, depth - 1);
            }
        }
        searchedMoves++;

        board.unmake();

        // 6. If we found a better move, update our bestScore/Move and principal variation
        if (score > bestScore) {
            bestScore = score;
            bestMove  = move;
            thread.pv.update(move, thread.depth);
        }

        // 7. Alpha-beta pruning
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            // Beta cut-off, update heuristics if quiet move
            thread.heuristics.updateBetaCutoff(board, move, thread.depth);

            if (thread.options.debug) {
                thread.stats.cutoffs[thread.depth]++;
                if (move == moves[0])
                    thread.stats.failHighEarly[thread.depth]++;
                else
                    thread.stats.failHighLate[thread.depth]++;
            }
            break;
        }
    }

    // Mate and draw detection
    if (legalMoves == 0) {
        if (board.isCheck()) {
            int score = -MATE_VALUE + thread.depth;
            TT::table.store(key, Move(), score, depth, TT::EXACT);
            return score;
        } else {
            TT::table.store(key, Move(), 0, depth, TT::EXACT);
            return 0;
        }
    }

    // 8. Store result in transposition table
    TT::NodeType flag = TT::EXACT;
    if (bestScore <= lowerbound)
        flag = TT::UPPERBOUND;
    else if (bestScore >= upperbound)
        flag = TT::LOWERBOUND;
    TT::table.store(key, bestMove, bestScore, depth, flag);

    return bestScore;
}

template int search<NodeType::Root>(Thread&, int, int, int);

int quiescence(Thread& thread, int alpha, int beta) {
    // 1. Evaluate the current position (stand-pat).
    Board& board = thread.board;
    int standPat = eval(board);
    thread.stats.totalNodes++;
    if (thread.options.debug) {
        thread.stats.nodes[thread.depth]++;
        thread.stats.qNodes[thread.depth]++;
    }

    if (standPat >= beta) {
        // beta cutoff, return upperbound
        return beta;
    }

    if (standPat > alpha) {
        // update alpha/lowerbound if position improves it
        alpha = standPat;
    }

    // 2. Generate only forcing moves and sort by priority
    MoveGenerator<GenType::Captures> moves{board};
    moves.sort(MovePriority(thread, NodeType::NonPV));

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
            // beta cutoff, return upperbound
            if (thread.options.debug) {
                thread.stats.cutoffs[thread.depth]++;
                if (move == moves[0])
                    thread.stats.failHighEarly[thread.depth]++;
                else
                    thread.stats.failHighLate[thread.depth]++;
            }
            return beta;
        }
        if (score > alpha) {
            // update alpha/lowerbound
            alpha = score;
        }
    }

    if (legalMoves == 0) {
        if (board.isCheck())
            return -MATE_VALUE + thread.depth;
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
