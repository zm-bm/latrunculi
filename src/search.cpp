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
    int bestScore  = -MATESCORE;
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
            if (entry->flag == TT::EXACT) {
                thread.pv.update(entry->bestMove, thread.depth);
                if (thread.options.debug) thread.stats.ttCutoffs[thread.depth]++;
                return entry->score;
            }
            if (entry->flag == TT::LOWERBOUND && entry->score >= beta) {
                if (thread.options.debug) thread.stats.ttCutoffs[thread.depth]++;
                return entry->score;
            }
            if (entry->flag == TT::UPPERBOUND && entry->score <= alpha) {
                if (thread.options.debug) thread.stats.ttCutoffs[thread.depth]++;
                return entry->score;
            }
        }
    }

    // 3. Generate moves and sort by priority
    MoveGenerator<GenType::All> moves{board};
    moves.sort(MovePriority(thread, node, entry));

    // TODO: handle draws, for now just return eval
    if (moves.empty()) return eval(board);

    // 4. Loop over moves
    int movesSearched = 0;
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
        bool isQuiet = !board.isCapture(move) && !board.isCheckingMove(move);

        // Futility pruning
        if (depth <= 2 && isQuiet) {
            int staticEval = eval(board);
            if (staticEval + (FutilityMargin * depth) <= alpha) continue;
        }

        // 5. Recursively search
        board.make(move);

        int score;
        bool doFullSearch = isRoot || (isPV && movesSearched == 0);
        if (doFullSearch) {
            score = -search<NodeType::PV>(thread, -beta, -alpha, depth - 1);
        } else {
            // LMR + null window search, re-search if it fail-high
            bool doReduce = (movesSearched >= FullDepthMoves) && (depth >= ReductionLimit);
            int reduction =
                (doReduce && !isPV && isQuiet) ? 1 + std::min(movesSearched / 10, depth / 4) : 0;

            score = -search<NodeType::NonPV>(thread, -alpha - 1, -alpha, depth - 1 - reduction);
            if (score > alpha) {
                score = -search<node>(thread, -beta, -alpha, depth - 1);
            }
        }
        movesSearched++;

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

    // 8. Store result in transposition table
    TT::NodeType flag = TT::EXACT;
    if (bestScore <= lowerbound)
        flag = TT::UPPERBOUND;
    else if (bestScore >= upperbound)
        flag = TT::LOWERBOUND;
    TT::table.store(key, bestMove, bestScore, depth, flag);

    // TODO: handle no legal moves, 50 move rule, draw by repetition, etc
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

    // TODO: handle draws, for now just return standPat
    if (moves.empty()) return standPat;

    // 3. Loop over moves
    for (auto& move : moves) {
        if (!board.isLegalMove(move)) continue;
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

    // TODO: handle no legal moves, 50 move rule, draw by repetition, etc
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
