#ifndef LATRUNCULI_EVALUATOR_H
#define LATRUNCULI_EVALUATOR_H

#include <string>

#include "board.hpp"
#include "chess.hpp"
#include "eval.hpp"
#include "score.hpp"

class Evaluator {
   public:
    Evaluator(const Chess& c) : chess{c}, board{c.board} {}

    template <bool>
    int eval() const;

    Score pawnsEval() const;
    int isolatedPawnsCount() const;
    int backwardsPawnsCount() const;
    int doubledPawnsCount() const;

    // Score piecesEval() const;

    // template <Color c>
    // U64 outpostSquares() const;
    // int minorsBehindPawns() const;

    // int knightOutpostCount() const;
    // int bishopOutpostCount() const;
    // int bishopPawnsScore() const;

    U64 passedPawns(Color) const;
    bool oppositeBishops() const;
    int phase() const;
    int nonPawnMaterial(Color) const;
    int scaleFactor() const;

   private:
    const Chess& chess;
    const Board& board;
};

inline Score Evaluator::pawnsEval() const {
    Score score = {0, 0};

    score += ISO_PAWN_PENALTY * isolatedPawnsCount();
    score += BACKWARD_PAWN_PENALTY * backwardsPawnsCount();
    score += DOUBLED_PAWN_PENALTY * doubledPawnsCount();

    return score;
}

inline int Evaluator::isolatedPawnsCount() const {
    U64 wIsolatedPawns = Eval::isolatedPawns(board.getPieces<PAWN>(WHITE));
    U64 bIsolatedPawns = Eval::isolatedPawns(board.getPieces<PAWN>(BLACK));
    return BB::bitCount(wIsolatedPawns) - BB::bitCount(bIsolatedPawns);
}

inline int Evaluator::backwardsPawnsCount() const {
    U64 wPawns = board.getPieces<PAWN>(WHITE);
    U64 bPawns = board.getPieces<PAWN>(BLACK);
    U64 wBackwardsPawns = Eval::backwardsPawns<WHITE>(wPawns, bPawns);
    U64 bBackwardsPawns = Eval::backwardsPawns<BLACK>(bPawns, wPawns);
    return BB::bitCount(wBackwardsPawns) - BB::bitCount(bBackwardsPawns);
}

inline int Evaluator::doubledPawnsCount() const {
    U64 wDoubledPawns = Eval::doubledPawns<WHITE>(board.getPieces<PAWN>(WHITE));
    U64 bDoubledPawns = Eval::doubledPawns<BLACK>(board.getPieces<PAWN>(BLACK));
    return BB::bitCount(wDoubledPawns) - BB::bitCount(bDoubledPawns);
}

inline bool Evaluator::oppositeBishops() const {
    if (board.count<BISHOP>(WHITE) != 1 || board.count<BISHOP>(BLACK) != 1) {
        return false;
    }
    U64 wBishops = board.getPieces<BISHOP>(WHITE);
    U64 bBishops = board.getPieces<BISHOP>(BLACK);
    return (((wBishops & LIGHT_SQUARES) && (bBishops & DARK_SQUARES)) ||
            ((wBishops & DARK_SQUARES) && (bBishops & LIGHT_SQUARES)));
}

inline int Evaluator::phase() const {
    int totalNpm = nonPawnMaterial(WHITE) + nonPawnMaterial(BLACK);
    int material = std::clamp(totalNpm, EG_LIMIT, MG_LIMIT);
    return ((material - EG_LIMIT) * PHASE_LIMIT) / (MG_LIMIT - EG_LIMIT);
}

inline int Evaluator::nonPawnMaterial(Color c) const {
    return (board.count<KNIGHT>(c) * Eval::pieceValue(MIDGAME, WHITE, KNIGHT) +
            board.count<BISHOP>(c) * Eval::pieceValue(MIDGAME, WHITE, BISHOP) +
            board.count<ROOK>(c) * Eval::pieceValue(MIDGAME, WHITE, ROOK) +
            board.count<QUEEN>(c) * Eval::pieceValue(MIDGAME, WHITE, QUEEN));
}

// template <Color c, typename Func>
// void forEach(U64 bitboard, Func action) {
//     while (bitboard) {
//         Square sq = BB::advanced<WHITE>(bitboard);
//         action(sq);
//         bitboard &= BB::clear(sq);
//     }
// }

// Score Evaluator::piecesEval() const {
//     Score score = {0, 0};
//     U64 pieces;
//     U64 occ = board.occupancy();
//     U64 wOutposts = outpostSquares<WHITE>();
//     U64 bOutposts = outpostSquares<BLACK>();

//     score += KNIGHT_OUTPOST_BONUS * knightOutpostCount();
//     score += BISHOP_OUTPOST_BONUS * bishopOutpostCount();
//     score += MINOR_BEHIND_PAWN_BONUS * minorsBehindPawns();
//     // score += BISHOP_PAWNS_PENALTY * bishopPawnsScore();

//     // white knights
//     forEach<WHITE>(board.getPieces<KNIGHT>(WHITE), [&](Square sq) {
//         score +=
//             REACHABLE_OUTPOST_BONUS * BB::bitCount(BB::movesByPiece<KNIGHT>(sq, occ) &
//             wOutposts);
//     });

//     // black knights
//     forEach<BLACK>(board.getPieces<KNIGHT>(BLACK), [&](Square sq) {
//         score -=
//             REACHABLE_OUTPOST_BONUS * BB::bitCount(BB::movesByPiece<KNIGHT>(sq, occ) &
//             bOutposts);
//     });

//     return score;
// }

// template <Color c>
// inline U64 Evaluator::outpostSquares() const {
//     constexpr Color enemy = ~c;
//     return Eval::outpostSquares<c>(board.getPieces<PAWN>(c), board.getPieces<PAWN>(enemy));
// }

// inline int Evaluator::minorsBehindPawns() const {
//     U64 wPawnsInFront = BB::movesByPawns<PawnMove::PUSH, BLACK>(board.getPieces<PAWN>(WHITE));
//     U64 bPawnsInFront = BB::movesByPawns<PawnMove::PUSH, WHITE>(board.getPieces<PAWN>(BLACK));
//     U64 wMinors = board.getPieces<KNIGHT>(WHITE) | board.getPieces<BISHOP>(WHITE);
//     U64 bMinors = board.getPieces<KNIGHT>(BLACK) | board.getPieces<BISHOP>(BLACK);
//     U64 wMinorsBehind = wMinors & wPawnsInFront;
//     U64 bMinorsBehind = bMinors & bPawnsInFront;
//     return BB::bitCount(wMinorsBehind) - BB::bitCount(bMinorsBehind);
// }



// inline int Evaluator::knightOutpostCount() const {
//     U64 wOutposts = outpostSquares<WHITE>();
//     U64 bOutposts = outpostSquares<BLACK>();
//     return (BB::bitCount(board.getPieces<KNIGHT>(WHITE) & wOutposts) -
//             BB::bitCount(board.getPieces<KNIGHT>(BLACK) & bOutposts));
// }

// inline int Evaluator::bishopOutpostCount() const {
//     U64 wOutposts = outpostSquares<WHITE>();
//     U64 bOutposts = outpostSquares<BLACK>();
//     return (BB::bitCount(board.getPieces<BISHOP>(WHITE) & wOutposts) -
//             BB::bitCount(board.getPieces<BISHOP>(BLACK) & bOutposts));
// }

// inline int Evaluator::bishopPawnsScore() const {
//     U64 wPawns = board.getPieces<PAWN>(WHITE);
//     U64 bPawns = board.getPieces<PAWN>(BLACK);
//     return (Eval::bishopPawns<WHITE>(board.getPieces<BISHOP>(WHITE), wPawns, bPawns) -
//             Eval::bishopPawns<BLACK>(board.getPieces<BISHOP>(BLACK), bPawns, wPawns));
// }

#endif
