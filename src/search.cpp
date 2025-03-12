#include "search.hpp"

#include <algorithm>

#include "chess.hpp"
#include "movegen.hpp"
#include "thread.hpp"
#include "tt.hpp"

namespace Search {

int quiescence(Thread& th, int alpha, int beta) {
    // 1. Evaluate the current position (stand-pat).
    int standPat = th.chess.eval<false>();

    // If our evaluation already beats beta, we can cut off.
    if (standPat >= beta) {
        return beta;
    }

    // Otherwise, raise alpha if this position is already better than alpha.
    if (standPat > alpha) {
        alpha = standPat;
    }

    // 2. Generate only forcing moves and sort by priority
    MoveGenerator<GenType::Captures> moves{th.chess};
    moves.sort(MovePriority(th), false);

    if (moves.empty()) return standPat;

    // 3. Loop over moves
    for (auto& move : moves) {
        if (!th.chess.isLegalMove(move)) continue;

        th.chess.make(move);
        int score = -quiescence(th, -beta, -alpha);
        th.chess.unmake();

        if (score >= beta) {
            return beta;  // Beta cutoff
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    // TODO: handle no legal moves, 50 move rule, draw by repetition, etc
    return alpha;
}

template <bool root>
int negamax(Thread& th, int alpha, int beta, int depth) {
    // 1. Base case: leaf node or no moves
    if (depth == 0) {
        return quiescence(th, alpha, beta);
    }

    Chess& chess = th.chess;
    bool isPV = (beta - alpha > 1);

    // 2. Generate moves and sort by priority
    MoveGenerator<GenType::All> moves{chess};
    moves.sort(MovePriority(th), isPV);

    if (moves.empty()) return chess.eval<false>();

    // 3. Loop over moves
    int bestScore = -MATESCORE;
    for (auto& move : moves) {
        if (!chess.isLegalMove(move)) continue;

        // Make the move
        chess.make(move);

        // Recursively search
        int score = -negamax<false>(th, -beta, -alpha, depth - 1);

        // Unmake the move
        chess.unmake();

        // 4. If we found a better move, update our bestScore and pv
        if (score > bestScore) {
            bestScore = score;
            th.pvTable[th.currentDepth].update(move, th.pvTable[th.currentDepth + 1]);
        }

        // 5. Alpha-beta pruning
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            // Beta cut-off, update heuristics if quiet move
            if (chess.pieceOn(move.to()) == Piece::NONE) {
                th.killers[th.currentDepth].update(move);
                th.history.update(chess.sideToMove(), move.from(), move.to(), th.currentDepth);
            }
            break;
        }
    }

    // TODO: handle no legal moves, 50 move rule, draw by repetition, etc
    return bestScore;
}

template int negamax<true>(Thread&, int, int, int);
template int negamax<false>(Thread&, int, int, int);

template <bool Root>
U64 perft(int depth, Chess& chess, std::ostream& oss) {
    if (depth == 0) return 1;

    MoveGenerator<GenType::All> moves{chess};

    U64 count = 0, nodes = 0;

    for (auto& move : moves) {
        if (!chess.isLegalMove(move)) continue;

        chess.make(move);

        count = perft<false>(depth - 1, chess);
        nodes += count;

        if (Root) oss << move << ": " << count << '\n';

        chess.unmake();
    }

    if constexpr (Root) {
        oss << "NODES: " << nodes << std::endl;
    }

    return nodes;
}

template U64 perft<true>(int, Chess&, std::ostream&);
// template U64 perft<true, false>(int, Chess&);

}  // namespace Search
