#pragma once

#include <array>

#include "bb.hpp"
#include "constants.hpp"
#include "score.hpp"
#include "types.hpp"

namespace Eval {

inline int psqValue(Phase ph, Color c, PieceType pt, Square sq) {
    int score = PSQ_VALUES[pt - 1][ph][SQUARE_MAP[c][sq]];
    return (2 * c * score) - score;
}

template <Color c>
inline U64 passedPawns(U64 pawns, U64 enemyPawns) {
    constexpr Color enemy = ~c;
    return pawns & ~BB::pawnFullSpan<enemy>(enemyPawns);
}

inline U64 isolatedPawns(U64 pawns) {
    U64 pawnsFill = BB::fillFiles(pawns);
    return (pawns & ~BB::shiftWest(pawnsFill)) & (pawns & ~BB::shiftEast(pawnsFill));
}

template <Color c>
inline U64 backwardsPawns(U64 pawns, U64 enemyPawns) {
    constexpr Color enemy = ~c;
    U64 stops = BB::pawnMoves<PUSH, c>(pawns);
    U64 attackSpan = BB::pawnAttackSpan<c>(pawns);
    U64 enemyAttacks = BB::pawnAttacks<enemy>(enemyPawns);
    return BB::pawnMoves<PUSH, enemy>(stops & enemyAttacks & ~attackSpan);
}

template <Color c>
inline U64 doubledPawns(U64 pawns) {
    // pawns that have friendly pawns behind them that aren't supported
    U64 pawnsBehind = pawns & BB::spanFront<c>(pawns);
    U64 supported = BB::pawnAttacks<c>(pawns);
    return pawnsBehind & ~supported;
}

template <Color c>
inline U64 blockedPawns(U64 pawns, U64 occupancy) {
    constexpr Color enemy = ~c;
    return pawns & BB::pawnMoves<PUSH, enemy>(occupancy);
}

template <Color c>
inline U64 outpostSquares(U64 pawns, U64 enemyPawns) {
    constexpr U64 rankMask = (c == WHITE) ? WHITE_OUTPOSTS : BLACK_OUTPOSTS;
    constexpr Color enemy = ~c;
    U64 holes = ~BB::pawnAttackSpan<enemy>(enemyPawns) & rankMask;
    return holes & BB::pawnAttacks<c>(pawns);
}

}  // namespace Eval
