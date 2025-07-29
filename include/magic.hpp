/**
 * Header file for magic move bitboard generation.
 *
 * This file uses magic numbers and related constants from Pradyumna Kannan's work.
 * The implementation code including move generation functions and
 * initialization routines are original to Latrunculi.
 *
 * The magic keys are not optimal for all squares but they are very close
 * to optimal.
 *
 * Copyright (C) 2007 Pradyumna Kannan.
 *
 * This code is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this code. Permission is granted to anyone to use this
 * code for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this code must not be misrepresented; you must not
 * claim that you wrote the original code. If you use this code in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original code.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#pragma once

#include "defs.hpp"

namespace magic {

extern uint64_t        rook_table[102400];
extern uint64_t        bishop_table[5248];
extern uint64_t* const rook_moves_table[64];
extern uint64_t* const bishop_moves_table[64];
extern const uint64_t  rook_magic[64];
extern const uint64_t  bishop_magic[64];
extern const uint64_t  rook_mask[64];
extern const uint64_t  bishop_mask[64];
extern const int       rook_shift[64];
extern const int       bishop_shift[64];

void init();

inline uint64_t rook_moves(Square sq, uint64_t occupied) {
    const auto occ   = occupied & rook_mask[sq];
    const auto index = (occ * rook_magic[sq]) >> rook_shift[sq];
    return rook_moves_table[sq][index];
}

inline uint64_t bishop_moves(Square sq, uint64_t occupied) {
    const auto occ   = occupied & bishop_mask[sq];
    const auto index = (occ * bishop_magic[sq]) >> bishop_shift[sq];
    return bishop_moves_table[sq][index];
}

inline uint64_t queen_moves(Square sq, uint64_t occupied) {
    return bishop_moves(sq, occupied) | rook_moves(sq, occupied);
}

} // namespace magic
