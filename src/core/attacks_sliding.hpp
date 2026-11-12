/**
 * Header file for PEXT and magic sliding attack generation.
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

#include <array>
#include <cstddef>
#include <cstdint>

#if LATRUNCULI_USE_PEXT
#include <immintrin.h>
#endif

#include "core/piece.hpp"
#include "core/square.hpp"

namespace attacks::sliding {

namespace detail {

template <typename T>
using SquareTable = std::array<T, N_SQUARES>;

// Slice pointers remain constant; init() fills the pointed-to attack storage.
extern const SquareTable<Bitboard*> rook_attacks;
extern const SquareTable<Bitboard*> bishop_attacks;

// Per-square blocker masks, hash multipliers, and shifts.
extern const SquareTable<std::uint64_t> rook_magic;
extern const SquareTable<std::uint64_t> bishop_magic;
extern const SquareTable<Bitboard> rook_masks;
extern const SquareTable<Bitboard> bishop_masks;
extern const SquareTable<int> rook_shifts;
extern const SquareTable<int> bishop_shifts;

// Initialization and queries must agree on the table's index order.
template <PieceType P>
inline std::size_t occupancy_index(Square sq, Bitboard occupied) noexcept {
    static_assert(P == BISHOP || P == ROOK);
    const auto& masks = P == BISHOP ? bishop_masks : rook_masks;
#if LATRUNCULI_USE_PEXT
    return _pext_u64(occupied, masks[sq]);
#else
    const auto& multipliers = P == BISHOP ? bishop_magic : rook_magic;
    const auto& shifts = P == BISHOP ? bishop_shifts : rook_shifts;
    return ((occupied & masks[sq]) * multipliers[sq]) >> shifts[sq];
#endif
}

} // namespace detail

// Populates the shared tables once at startup, before any slider lookups.
void init();

inline Bitboard rook_moves(Square sq, Bitboard occupied) noexcept {
    return detail::rook_attacks[sq][detail::occupancy_index<ROOK>(sq, occupied)];
}

inline Bitboard bishop_moves(Square sq, Bitboard occupied) noexcept {
    return detail::bishop_attacks[sq][detail::occupancy_index<BISHOP>(sq, occupied)];
}

// Queen slider attacks are the union of bishop and rook lookups.
inline Bitboard queen_moves(Square sq, Bitboard occupied) noexcept {
    return bishop_moves(sq, occupied) | rook_moves(sq, occupied);
}

// Empty-board rays always occupy entry zero, without index calculations.
inline Bitboard rook_rays(Square sq) noexcept {
    return detail::rook_attacks[sq][0];
}

inline Bitboard bishop_rays(Square sq) noexcept {
    return detail::bishop_attacks[sq][0];
}

inline Bitboard queen_rays(Square sq) noexcept {
    return bishop_rays(sq) | rook_rays(sq);
}

} // namespace attacks::sliding
