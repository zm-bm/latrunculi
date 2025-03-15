#include "search.hpp"

#include <algorithm>

#include "board.hpp"
#include "movegen.hpp"
#include "thread.hpp"
#include "tt.hpp"
#include "eval.hpp"

namespace Search {

int quiescence(Thread& th, int alpha, int beta) {
    // 1. Evaluate the current position (stand-pat).
    int standPat = eval(th.chess);

    if (standPat >= beta) {
        // beta cutoff, return upperbound
        return beta;
    }

    if (standPat > alpha) {
        // update alpha/lowerbound if position improves it
        alpha = standPat;
    }

    // 2. Generate only forcing moves and sort by priority
    MoveGenerator<GenType::Captures> moves{th.chess};
    moves.sort(MovePriority(th, NodeType::NonPV));

    // TODO: handle draws, for now just return standPat
    if (moves.empty()) return standPat;

    // 3. Loop over moves
    for (auto& move : moves) {
        if (!th.chess.isLegalMove(move)) continue;

        // 4. Recursively search
        th.chess.make(move);
        int score = -quiescence(th, -beta, -alpha);
        th.chess.unmake();

        if (score >= beta) {
            // beta cutoff, return upperbound
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
int search(Thread& th, int alpha, int beta, int depth) {
    // 1. Base case: quiescence search
    if (depth == 0) {
        return quiescence(th, alpha, beta);
    }

    constexpr bool isPV = (node == NodeType::Root || node == NodeType::PV);
    Board& chess = th.chess;
    U64 key = chess.getKey();
    int lowerbound = alpha;
    int upperbound = beta;
    int bestScore = -MATESCORE;
    Move bestMove;

    // 2. Check the transposition table
    TT::Entry* entry;
    if constexpr (!isPV) {
        entry = TT::table.probe(key);
        if (entry->isValid(key) && entry->depth >= depth) {
            if (entry->flag == TT::EXACT) {
                th.pv.update(entry->bestMove, th.currentDepth);
                return entry->score;
            }
            if (entry->flag == TT::LOWERBOUND && entry->score >= beta) return entry->score;
            if (entry->flag == TT::UPPERBOUND && entry->score <= alpha) return entry->score;
        }
    }

    // 3. Generate moves and sort by priority
    MoveGenerator<GenType::All> moves{chess};
    moves.sort(MovePriority(th, node, entry));

    // TODO: handle draws, for now just return eval
    if (moves.empty()) return eval(chess);

    // 4. Loop over moves
    for (auto& move : moves) {
        if (!chess.isLegalMove(move)) continue;

        // 5. Recursively search
        chess.make(move);

        int score;
        if constexpr (node == NodeType::Root) {
            // Root moves always get a full search
            score = -search<NodeType::PV>(th, -beta, -alpha, depth - 1);
        } else if (node == NodeType::PV && move == moves[0]) {
            // First move of a PV node gets a full search
            score = -search<NodeType::PV>(th, -beta, -alpha, depth - 1);
        } else {
            // Null window search, re-search if it fails high
            score = -search<NodeType::NonPV>(th, -alpha - 1, -alpha, depth - 1);
            if (score > alpha) {
                score = -search<node>(th, -beta, -alpha, depth - 1);
            }
        }

        chess.unmake();

        // 6. If we found a better move, update our bestScore/Move and principal variation
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
            th.pv.update(move, th.currentDepth);
        }

        // 7. Alpha-beta pruning
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            // Beta cut-off, update heuristics if quiet move
            th.heuristics.updateBetaCutoff(chess, move, th.currentDepth);
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

template <NodeType node>
U64 perft(int depth, Board& chess, std::ostream& oss) {
    if (depth == 0) return 1;

    MoveGenerator<GenType::All> moves{chess};

    U64 count = 0, nodes = 0;

    for (auto& move : moves) {
        if (!chess.isLegalMove(move)) continue;

        chess.make(move);

        count = perft<NodeType::NonPV>(depth - 1, chess);
        nodes += count;

        if constexpr (node == NodeType::Root) {
            oss << move << ": " << count << '\n';
        }

        chess.unmake();
    }

    if constexpr (node == NodeType::Root) {
        oss << "NODES: " << nodes << std::endl;
    }

    return nodes;
}

template U64 perft<NodeType::Root>(int, Board&, std::ostream& = std::cout);

}  // namespace Search
