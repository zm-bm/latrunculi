#include "search.hpp"

#include <algorithm>

#include "chess.hpp"
#include "movegen.hpp"
#include "tt.hpp"

using namespace std::chrono;

namespace Search {

U64 nodeCount = 0;
high_resolution_clock::time_point startTime;

int quiescence(Chess& chess, int alpha, int beta) {
    // 1. Evaluate the current position (stand-pat).
    int standPat = chess.eval<false>();

    // If our evaluation already beats beta, we can cut off.
    if (standPat >= beta) {
        return beta;
    }

    // Otherwise, raise alpha if this position is already better than alpha.
    if (standPat > alpha) {
        alpha = standPat;
    }

    // 2. Generate only forcing moves
    MoveGenerator<GenType::Captures> moves(&chess);
    if (moves.empty()) return standPat;

    for (auto& move : moves) {
        if (!chess.isLegalMove(move)) continue;

        chess.make(move);
        nodeCount += 1;
        int score = -quiescence(chess, -beta, -alpha);
        chess.unmake();

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
Result negamax(Chess& chess, int alpha, int beta, int depth) {
    // 1. Base case: leaf node or no moves
    if (depth == 0) {
        return {quiescence(chess, alpha, beta), {}};
    }

    MoveGenerator<GenType::Legal> moves(&chess);
    if (moves.empty()) return {chess.eval<false>(), {}};

    // 2. Initialize the best result
    Result bestResult;
    bestResult.score = -MATESCORE;

    // 3. Loop over moves
    for (auto& move : moves) {
        if (!chess.isLegalMove(move)) continue;

        // Make the move
        chess.make(move);
        nodeCount += 1;

        // Recursively search
        Result result = negamax<false>(chess, -beta, -alpha, depth - 1);
        result.score = -result.score;

        // Unmake the move
        chess.unmake();

        // 4. If we found a better move, update our bestResult
        if (result.score > bestResult.score) {
            bestResult.score = result.score;

            // Build the new principal variation:
            bestResult.pv.clear();
            bestResult.pv.push_back(move);
            bestResult.pv.insert(bestResult.pv.end(), result.pv.begin(), result.pv.end());

            if constexpr (root) {
                std::cout << generateUCILine(depth, bestResult) << std::endl;
            }
        }

        // 5. Alpha-beta pruning
        alpha = std::max(alpha, result.score);
        if (alpha >= beta) {
            // Beta cut-off
            break;
        }
    }

    // TODO: handle no legal moves, 50 move rule, draw by repetition, etc
    return bestResult;
}

template Result negamax<true>(Chess&, int, int, int);
template Result negamax<false>(Chess&, int, int, int);

std::string generateUCILine(int depth, Result result) {
    std::ostringstream oss;
    auto dur = high_resolution_clock::now() - startTime;
    auto sec = duration_cast<duration<double>>(dur).count();
    auto nps = (sec > 0) ? nodeCount / sec : 0;

    oss << "info depth " << depth;
    oss << " score cp " << result.score;
    oss << " time " << std::fixed << std::setprecision(1) << sec;
    oss << " nodes " << nodeCount;
    oss << " nps " << std::setprecision(0) << nps;
    oss << " pv";
    for (auto& move : result.pv) oss << " " << move;

    return oss.str();
}

template <bool Root, bool ShowOutput = true>
U64 perft(int depth, Chess& chess) {
    if (depth == 0) return 1;

    MoveGenerator<GenType::Legal> moves(&chess);

    U64 count = 0, nodes = 0;

    for (auto& move : moves) {
        if (!chess.isLegalMove(move)) continue;

        chess.make(move);

        count = perft<false, false>(depth - 1, chess);
        nodes += count;

        if (Root && ShowOutput) std::cout << move << ": " << count << std::endl;

        chess.unmake();
    }

    if constexpr (Root && ShowOutput) {
        std::cout << "NODES: " << nodes << std::endl;
    }

    return nodes;
}

template U64 perft<true>(int, Chess&);
template U64 perft<true, false>(int, Chess&);

}  // namespace Search

// void Search::think(int depth) {
//     reset();

//     for (int i = 1; i < depth + 1; i++) {
//         bestScore = negamax<true>(i, -MATESCORE, MATESCORE);

//         if (abs(bestScore) > MATESCORE - 1000) break;
//         if (bestMove.isNullMove()) break;
//     }

//     std::cout << "bestmove " << bestMove << std::endl;
// }

// template <bool Root>
// int Search::negamax(int depth, int alpha, int beta, bool isPV, bool isNullAllowed) {
//     int score = 0;
//     int bestScoreSoFar = -MATESCORE;
//     Move bestMoveSoFar = Move();
//     NodeType ttType = TT_ALPHA;

//     if (Root) {
//         nSearched = 0;
//         searchPly = 0;
//     }

//     // Clear the line
//     pv[searchPly].clear();

//     // If in check, search deeper
//     bool wasInCheck = _board->isCheck();
//     if (wasInCheck)
//         depth += 1;

//     // If we've reach max depth, begin static evaluation of the board
//     if (depth == 0)
//         return quiesce(alpha, beta);

//     // First check the transposition table
//     Move hashMove = Move();
//     TT::Entry* entry = nullptr;
//     if (!Root) {
//         entry = TT::table.probe(_board->getKey());

//         if (entry) {
//             // If we have a table hit, use hash move for move ordering
//             hashMove = entry->best;
//             int hashScore = 0;

//             // If an exact hit with sufficient depth,
//             // we aren't in a PV node and score is inside the search window:
//             // Then return the previous score
//             if (entry->flag == TT_EXACT
//                 && entry->depth >= depth)
//             {
//                 hashScore = entry->score;
//                 if (!isPV || (alpha < hashScore && hashScore < beta))
//                     return hashScore;
//             }

//             // Otherwise return if upper or lower bound in the TT
//             // is able to produce a cutoff
//             else if (entry->flag == TT_ALPHA
//                      && entry->score <= alpha
//                      && entry->depth >= depth)
//             {
//                 hashScore = alpha;
//                 if (!isPV)
//                     return hashScore;
//             }
//             else if (entry->flag == TT_BETA
//                      && entry->score >= beta
//                      && entry->depth >= depth)
//             {
//                 hashScore = beta;
//                 if (!isPV)
//                     return hashScore;
//             }

//         }

//         // Null move pruning
//         // Allow opponent to make two moves in a row, and
//         // search for a beta cutoff at a reduced depth, R
//         // Avoid if in zugzwang
//         int R = 3;
//         if (depth > 7)
//             R = 4;
//         if (isNullAllowed
//             && depth > R
//             && !isPV
//             && !wasInCheck
//             && _board->getPieceCount(_board->turn) > 0)
//         {
//             _board->makeNull();
//             ++searchPly;
//             score = -negamax<false>(depth-R-1, -beta, -beta+1, false, false);
//             --searchPly;
//             _board->unmmakeNull();

//             if (score >= beta)
//                 return beta;
//         }
//     }

//     // Generate and sort moves
//     int nLegalMoves = 0;
//     auto movegen = MoveGenerator(_board);
//     movegen.generatePseudoLegalMoves();
//     sortMoves(movegen.moves, hashMove);

//     // For each move
//     for (auto& move : movegen.moves)
//     {
//         // First check if move is legal
//         if (!_board->isLegalMove(move))
//             continue;
//         else {
//             nSearched++;
//             nLegalMoves++;
//         }

//         // Make the move
//         _board->make(move);
//         searchPly++;

//         // Late move reductions
//         // Search likely fail low nodes at a reduced depth
//         int lmrReduction = 0;
//         if (!Root) {
//             if (!isPV
//                 && depth > 3
//                 && nLegalMoves > 3
//                 && !wasInCheck
//                 && !_board->isCheck()
//                 && _board->state[_board->ply].captured == NO_PIECE_TYPE
//                 && move.type() != PROMOTION)
//             {
//                 if (nLegalMoves > 8)
//                     lmrReduction += 2;
//                 else
//                     lmrReduction += 1;
//             }
//         }

//         // PVS search
//         // Search first move, or PV move, with full window
//         int nextDepth = depth - lmrReduction - 1;
//         if (bestScoreSoFar == -MATESCORE)
//             score = -negamax<false>(nextDepth, -beta, -alpha);
//         // For remaining moves, search with null window centered around alpha
//         // in order to quickly check if it is an improvement, re-searching if so
//         else
//         {
//             score = -negamax<false>(nextDepth, -alpha-1, -alpha, isPV=false);
//             if ((score > alpha) && (score < beta))
//                 score = -negamax<false>(nextDepth, -beta, -alpha);
//         }
//         // If a search with LMR raises alpha, re-search to full depth
//         // since we expected a bad move
//         if (score > alpha && lmrReduction > 0)
//             score = -negamax<false>(depth-1, -beta, -alpha);
//         bestScoreSoFar = std::max(bestScoreSoFar, score);

//         // Undo the move on the board
//         searchPly--;
//         _board->unmake();

//         if (score > alpha)
//         {
//             // Update best move if score is above lower bound
//             bestMoveSoFar = move;

//             if (score >= beta)
//             {
//                 // If we have a beta cutoff, stop search since our opponent
//                 // has better available moves one ply up
//                 addToHistory(move, searchPly);
//                 ttType = TT_BETA;
//                 alpha = beta;
//                 if (Root)
//                     printPV(depth, beta, nSearched);
//                 break;
//             }

//             ttType = TT_EXACT;
//             alpha = score;
//             savePV(move);

//             if (Root)
//             {
//                 bestMove = move;
//                 printPV(depth, alpha, nSearched);
//             }
//         }
//     }

//     // Mate and draw detection
//     if (nLegalMoves == 0)
//     {
//         bestMoveSoFar = Move();
//         if (wasInCheck)
//             alpha = -MATESCORE + searchPly;
//         else
//             alpha = DRAWSCORE;
//     }
//     else if (_board->getHmClock() >= 100)
//         alpha = DRAWSCORE;

//     // Save search results in the transposition table
//     TT::table.save(_board->getKey(), depth, alpha, ttType, bestMoveSoFar);

//     return alpha;
// }

// template <bool Root, bool ShowOutput = true>
// U64 Search::perft(int depth) {
//     if (depth == 0) return 1;

//     if (Root && ShowOutput) start = high_resolution_clock::now();

//     auto movegen = MoveGenerator(chess);
//     movegen.generatePseudoLegalMoves();

//     U64 count = 0, nodes = 0;

//     for (auto& move : movegen.moves) {
//         if (!chess->isLegalMove(move)) continue;

//         chess->make(move);

//         count = perft<false>(depth - 1);
//         nodes += count;

//         if (Root && ShowOutput) std::cout << move << ": " << count << std::endl;

//         chess->unmake();
//     }

//     if (Root && ShowOutput) {
//         stop = high_resolution_clock::now();

//         duration<double> d = duration_cast<duration<double>>(stop - start);
//         std::cout << "TOTAL TIME OF SEARCH: " << d.count() << std::endl;
//         std::cout << "TOTAL NODES SEARCHED: " << nodes << std::endl;
//         std::cout << "NODES PER SECOND    : " << nodes / d.count() << std::endl;
//     }

//     return nodes;
// }

// void Search::reset() {
//     // Reset the PV collector
//     for (int i = 0; i < MAX_DEPTH; i++) pv[i].clear();

//     // Zero the history table
//     for (int c = 0; c < 2; c++) {
//         for (int p = 0; p < 6; p++) {
//             for (int sq = 0; sq < 64; sq++) {
//                 history[c][p][sq] = 0;
//             }
//         }
//     }

//     // Start the clock
//     start = high_resolution_clock::now();
// }

// void Search::addToHistory(Move move, int ply) {
//     PieceRole captPiece = _board->getPieceType(move.to());
//     if (captPiece == NO_PIECE_TYPE && move.type() != PROMOTION)
//     {
//         // Add to killer moves
//         killers[ply].move2 = killers[ply].move1;
//         killers[ply].move1 = move;

//         // Add to history table
//         PieceRole movePiece = _board->getPieceType(move.from());
//         history[_board->turn][movePiece][move.to()] += (U32)1 << (U64)ply; // ply^2

//         if (history[_board->turn][movePiece][move.to()] > 10000)
//         {
//             for (int c = 0; c < 2; c++) {
//                 for (int p = 0; p < 6; p++) {
//                     for (int sq = 0; sq < 64; sq++) {
//                         history[c][p][sq] >>= 2;
//                     }
//                 }
//             }
//         }
//     }
// }

// void Search::savePV(Move move) {
//     U32 ply = (U32)searchPly;

//     pv[ply].clear();
//     pv[ply].push_back(move);

//     std::copy(pv[ply + 1].begin(), pv[ply + 1].end(), std::back_inserter(pv[ply]));
// }

// void Search::printPV(int depth, int score, unsigned int nSearched) {
//     stop = high_resolution_clock::now();
//     duration<double> d = duration_cast<duration<double>>(stop - start);

//     std::cout << "info depth " << depth;
//     std::cout << " score cp " << score;
//     std::cout << " nodes " << nSearched;
//     std::cout << " nps " << (U32)(nSearched / d.count());
//     std::cout << " time " << (int)(d.count() * 1000) << "\n";

//     std::cout << "pv ";
//     for (auto& m : pv[0]) std::cout << m << " ";
//     std::cout << std::endl;
// }

// void Search::sortMoves(std::vector<Move>& moves, Move hashmove) {
//     for (auto& move : moves)
//     {
//         if (move == bestMove)
//             move.score += 10001;

//         if (move == hashmove && !hashmove.isNullMove())
//             move.score += 10000;

//         if (move.type() == PROMOTION)
//             move.score += 1000 + move.promoPiece();

//         PieceRole captPiece = _board->getPieceType(move.to());
//         if (captPiece != NO_PIECE_TYPE) {
//             PieceRole movePiece = _board->getPieceType(move.from());
//             move.score += Eval::PieceValues[captPiece][WHITE] - movePiece;
//         }
//         else {
//             PieceRole movePiece = _board->getPieceType(move.from());
//             move.score += (I32)history[_board->turn][movePiece][move.to()];
//         }

//         if (move == killers[searchPly].move1)
//             move.score += 100;

//         if (move == killers[searchPly].move2)
//             move.score += 50;
//     }

//     std::stable_sort(moves.rbegin(), moves.rend());
// }

// template U64 Search::perft<true>(int);
// template U64 Search::perft<true, false>(int);
