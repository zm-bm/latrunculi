#pragma once

#include <bit>
#include <cassert>

#include "core/types.hpp"

namespace bb {

namespace detail {

constexpr Bitboard file1_mask = 0x0101010101010101ULL;
constexpr Bitboard file8_mask = 0x8080808080808080ULL;

} // namespace detail

constexpr Bitboard set(const Square sq) {
    return 0x1ULL << sq;
}

template <typename... Squares>
constexpr Bitboard set(const Squares... sqs) {
    return (set(sqs) | ...);
}

constexpr Bitboard clear_mask(const Square sq) {
    return ~set(sq);
}

constexpr void add(Bitboard& bitboard, const Square sq) {
    bitboard |= set(sq);
}

constexpr void remove(Bitboard& bitboard, const Square sq) {
    bitboard &= clear_mask(sq);
}

constexpr void move(Bitboard& bitboard, const Square from, const Square to) {
    remove(bitboard, from);
    add(bitboard, to);
}

constexpr bool contains(const Bitboard bitboard, const Square sq) {
    return bitboard & set(sq);
}

constexpr Bitboard file(const File file) {
    return detail::file1_mask << file;
}

constexpr Bitboard rank(const Rank rank) {
    return 0xFFULL << (rank * 8);
}

constexpr int count(const Bitboard bitboard) {
    return std::popcount(bitboard);
}

constexpr bool is_many(const Bitboard bitboard) {
    return bitboard & (bitboard - 1);
}

constexpr bool is_one(const Bitboard bitboard) {
    return bitboard && !is_many(bitboard);
}

constexpr Bitboard lsb_mask(const Bitboard bitboard) {
    assert(bitboard);
    return bitboard & -bitboard;
}

constexpr Square lsb(const Bitboard bitboard) {
    assert(bitboard);
    return Square(__builtin_ctzll(bitboard));
}

constexpr Square msb(const Bitboard bitboard) {
    assert(bitboard);
    return Square(63 - __builtin_clzll(bitboard));
}

template <Color C>
constexpr Square frontmost(const Bitboard bitboard) {
    return (C == WHITE) ? msb(bitboard) : lsb(bitboard);
}

constexpr Square lsb_pop(Bitboard& bitboard) {
    const Square sq  = lsb(bitboard);
    bitboard        &= bitboard - 1;
    return sq;
}

constexpr Square msb_pop(Bitboard& bitboard) {
    const Square sq = msb(bitboard);
    remove(bitboard, sq);
    return sq;
}

template <Color C>
constexpr Square pop(Bitboard& bitboard) {
    return (C == WHITE) ? msb_pop(bitboard) : lsb_pop(bitboard);
}

template <Color C, typename Func>
inline void scan(Bitboard bitboard, Func action) {
    while (bitboard)
        action(bb::pop<C>(bitboard));
}

constexpr Bitboard fill_north(Bitboard bitboard) {
    bitboard |= (bitboard << 8);
    bitboard |= (bitboard << 16);
    bitboard |= (bitboard << 32);
    return bitboard;
}

constexpr Bitboard fill_south(Bitboard bitboard) {
    bitboard |= (bitboard >> 8);
    bitboard |= (bitboard >> 16);
    bitboard |= (bitboard >> 32);
    return bitboard;
}

constexpr Bitboard fill(Bitboard bitboard) {
    return fill_north(bitboard) | fill_south(bitboard);
}

constexpr Bitboard shift_south(Bitboard bitboard) {
    return bitboard >> 8;
}

constexpr Bitboard shift_north(Bitboard bitboard) {
    return bitboard << 8;
}

constexpr Bitboard shift_east(Bitboard bitboard) {
    return (bitboard << 1) & ~detail::file1_mask;
}

constexpr Bitboard shift_west(Bitboard bitboard) {
    return (bitboard >> 1) & ~detail::file8_mask;
}

constexpr Bitboard span_north(Bitboard bitboard) {
    return shift_north(fill_north(bitboard));
}

constexpr Bitboard span_south(Bitboard bitboard) {
    return shift_south(fill_south(bitboard));
}

template <Color C>
constexpr Bitboard span_front(Bitboard bitboard) {
    return (C == WHITE) ? span_north(bitboard) : span_south(bitboard);
}

template <Color C>
constexpr Bitboard span_back(Bitboard bitboard) {
    return (C == WHITE) ? span_south(bitboard) : span_north(bitboard);
}

template <Color C>
constexpr Bitboard attack_span(Bitboard bitboard) {
    Bitboard frontspan = span_front<C>(bitboard);
    return shift_west(frontspan) | shift_east(frontspan);
}

template <Color C>
constexpr Bitboard full_span(Bitboard bitboard) {
    Bitboard frontspan = span_front<C>(bitboard);
    return shift_west(frontspan) | shift_east(frontspan) | frontspan;
}

} // namespace bb
