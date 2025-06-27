// This file uses magic bitboard tables and constants from Pradyumna Kannan.
// See magic_tables.hpp/cpp for license and attribution details.

#pragma once

#include "magic_tables.hpp"
#include "types.hpp"

namespace Magic {

using namespace MagicTables;

void init();

constexpr U64 getRookAttacks(Square sq, U64 occupied) {
    auto occupancy = occupied & rookMask[sq];
    auto index     = (occupancy * rookMagic[sq]) >> rookShift[sq];
    return rookAttacks[sq][index];
}

constexpr U64 getBishopAttacks(Square sq, U64 occupied) {
    auto occupancy = occupied & bishopMask[sq];
    auto index     = (occupancy * bishopMagic[sq]) >> bishopShift[sq];
    return bishopAttacks[sq][index];
}

constexpr U64 getQueenAttacks(Square sq, U64 occupied) {
    return getBishopAttacks(sq, occupied) | getRookAttacks(sq, occupied);
}

}  // namespace Magic
