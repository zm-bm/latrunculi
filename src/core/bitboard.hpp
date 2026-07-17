#pragma once

#include <bit>
#include <cassert>

#include "core/types.hpp"

namespace bb {

// Edge and base-rank masks used to build file/rank masks and guard shifts.
inline constexpr Bitboard file1_mask = 0x0101010101010101ULL;
inline constexpr Bitboard file8_mask = 0x8080808080808080ULL;
inline constexpr Bitboard rank1_mask = 0xFFULL;

// Single-square mask.
constexpr Bitboard set(const Square sq) {
    return 0x1ULL << sq;
}

// Combined mask for multiple squares.
template <typename... Squares>
constexpr Bitboard set(const Squares... sqs) {
    return (set(sqs) | ...);
}

// Mask with sq cleared.
constexpr Bitboard clear_mask(const Square sq) {
    return ~set(sq);
}

// Sets sq in-place; idempotent.
constexpr void add(Bitboard& bitboard, const Square sq) {
    bitboard |= set(sq);
}

// Clears sq in-place; idempotent.
constexpr void remove(Bitboard& bitboard, const Square sq) {
    bitboard &= clear_mask(sq);
}

// Moves occupancy from one square to another using idempotent add/remove.
constexpr void move(Bitboard& bitboard, const Square from, const Square to) {
    remove(bitboard, from);
    add(bitboard, to);
}

// Tests whether sq is present.
constexpr bool contains(const Bitboard bitboard, const Square sq) {
    return bitboard & set(sq);
}

// Whole-file mask.
constexpr Bitboard file(const File file) {
    return file1_mask << file;
}

// Whole-rank mask.
constexpr Bitboard rank(const Rank rank) {
    return rank1_mask << (rank * 8);
}

// Rank mask from color C's side of the board.
template <Color C>
constexpr Bitboard relative_rank(const Rank rank) {
    return bb::rank(Rank(int(rank) ^ (int(~C) * 7)));
}

// Number of occupied squares.
constexpr int count(const Bitboard bitboard) {
    return std::popcount(bitboard);
}

// True when at least two bits are set.
constexpr bool is_many(const Bitboard bitboard) {
    return bitboard & (bitboard - 1);
}

// True when exactly one bit is set.
constexpr bool is_one(const Bitboard bitboard) {
    return bitboard && !is_many(bitboard);
}

// Requires nonzero input; returns the lowest set bit as a mask.
constexpr Bitboard lsb_mask(const Bitboard bitboard) {
    assert(bitboard);
    return bitboard & -bitboard;
}

// Requires nonzero input; returns the lowest occupied square.
constexpr Square lsb(const Bitboard bitboard) {
    assert(bitboard);
    return Square(__builtin_ctzll(bitboard));
}

// Requires nonzero input; returns the highest occupied square.
constexpr Square msb(const Bitboard bitboard) {
    assert(bitboard);
    return Square(63 - __builtin_clzll(bitboard));
}

// Frontmost occupied square from color C's perspective.
template <Color C>
constexpr Square frontmost(const Bitboard bitboard) {
    return (C == WHITE) ? msb(bitboard) : lsb(bitboard);
}

// Pops and returns the lowest occupied square.
constexpr Square lsb_pop(Bitboard& bitboard) {
    const Square sq  = lsb(bitboard);
    bitboard        &= bitboard - 1;
    return sq;
}

// Pops and returns the highest occupied square.
constexpr Square msb_pop(Bitboard& bitboard) {
    const Square sq = msb(bitboard);
    remove(bitboard, sq);
    return sq;
}

// Pops in front-to-back order from color C's perspective.
template <Color C>
constexpr Square pop(Bitboard& bitboard) {
    return (C == WHITE) ? msb_pop(bitboard) : lsb_pop(bitboard);
}

// Iterates occupied squares without mutating the caller's bitboard.
template <Color C, typename Func>
inline void scan(Bitboard bitboard, Func action) {
    while (bitboard)
        action(bb::pop<C>(bitboard));
}

// Fills every square north of each set bit, including the source bits.
constexpr Bitboard fill_north(Bitboard bitboard) {
    bitboard |= (bitboard << 8);
    bitboard |= (bitboard << 16);
    bitboard |= (bitboard << 32);
    return bitboard;
}

// Fills every square south of each set bit, including the source bits.
constexpr Bitboard fill_south(Bitboard bitboard) {
    bitboard |= (bitboard >> 8);
    bitboard |= (bitboard >> 16);
    bitboard |= (bitboard >> 32);
    return bitboard;
}

// Fills full files containing any set bit.
constexpr Bitboard fill(Bitboard bitboard) {
    return fill_north(bitboard) | fill_south(bitboard);
}

// One-rank south shift.
constexpr Bitboard shift_south(Bitboard bitboard) {
    return bitboard >> 8;
}

// One-rank north shift.
constexpr Bitboard shift_north(Bitboard bitboard) {
    return bitboard << 8;
}

// One-file east shift, dropping wraparound from file 8.
constexpr Bitboard shift_east(Bitboard bitboard) {
    return (bitboard << 1) & ~file1_mask;
}

// One-file west shift, dropping wraparound from file 1.
constexpr Bitboard shift_west(Bitboard bitboard) {
    return (bitboard >> 1) & ~file8_mask;
}

// Squares strictly north of each set bit.
constexpr Bitboard span_north(Bitboard bitboard) {
    return shift_north(fill_north(bitboard));
}

// Squares strictly south of each set bit.
constexpr Bitboard span_south(Bitboard bitboard) {
    return shift_south(fill_south(bitboard));
}

// Squares in front of each set bit from color C's perspective.
template <Color C>
constexpr Bitboard span_front(Bitboard bitboard) {
    return (C == WHITE) ? span_north(bitboard) : span_south(bitboard);
}

// Squares behind each set bit from color C's perspective.
template <Color C>
constexpr Bitboard span_back(Bitboard bitboard) {
    return (C == WHITE) ? span_south(bitboard) : span_north(bitboard);
}

// Pawn attack files in front of the supplied squares.
template <Color C>
constexpr Bitboard attack_span(Bitboard bitboard) {
    Bitboard frontspan = span_front<C>(bitboard);
    return shift_west(frontspan) | shift_east(frontspan);
}

// Front span plus adjacent pawn attack files.
template <Color C>
constexpr Bitboard full_span(Bitboard bitboard) {
    Bitboard frontspan = span_front<C>(bitboard);
    return shift_west(frontspan) | shift_east(frontspan) | frontspan;
}

} // namespace bb
