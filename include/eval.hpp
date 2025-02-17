#ifndef LATRUNCULI_EVAL_H
#define LATRUNCULI_EVAL_H

#include <array>

#include "bb.hpp"
#include "constants.hpp"
#include "score.hpp"
#include "types.hpp"

namespace Eval {

inline int pieceValue(Phase ph, Color c, PieceType pt) { return PIECE_VALUES[ph][c][pt]; }

inline int psqValue(Phase ph, Color c, PieceType pt, Square sq) {
    int score = PSQ_VALUES[pt - 1][ph][SQUARE_MAP[c][sq]];
    return (2 * c * score) - score;
}

template <Color c>
inline U64 passedPawns(U64 pawns, U64 enemyPawns) {
    constexpr Color enemy = ~c;
    return pawns & ~BB::getAllFrontSpan<enemy>(enemyPawns);
}

inline U64 isolatedPawns(U64 pawns) {
    U64 pawnsFill = BB::fillFiles(pawns);
    return (pawns & ~BB::shiftWest(pawnsFill)) & (pawns & ~BB::shiftEast(pawnsFill));
}

template <Color c>
inline U64 backwardsPawns(U64 pawns, U64 enemyPawns) {
    constexpr Color enemy = ~c;
    U64 stops = BB::movesByPawns<PawnMove::PUSH, c>(pawns);
    U64 attackSpan = BB::getFrontAttackSpan<c>(pawns);
    U64 enemyAttacks = BB::attacksByPawns<enemy>(enemyPawns);
    return BB::movesByPawns<PawnMove::PUSH, enemy>(stops & enemyAttacks & ~attackSpan);
}

template <Color c>
inline U64 doubledPawns(U64 pawns) {
    // pawns that have friendly pawns behind them that aren't supported
    U64 pawnsBehind = pawns & BB::spanFront<c>(pawns);
    U64 supported = BB::attacksByPawns<c>(pawns);
    return pawnsBehind & ~supported;
}

template <Color c>
inline U64 blockedPawns(U64 pawns, U64 occupancy) {
    constexpr Color enemy = ~c;
    return pawns & BB::movesByPawns<PawnMove::PUSH, enemy>(occupancy);
}

template <Color c>
inline U64 outpostSquares(U64 pawns, U64 enemyPawns) {
    constexpr U64 rankMask = (c == WHITE) ? WHITE_OUTPOSTS : BLACK_OUTPOSTS;
    constexpr Color enemy = ~c;
    U64 holes = ~BB::getFrontAttackSpan<enemy>(enemyPawns) & rankMask;
    return holes & BB::attacksByPawns<c>(pawns);
}

}  // namespace Eval

#endif