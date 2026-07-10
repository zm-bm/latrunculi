#pragma once

#include <cstdint>

#include "core/types.hpp"

namespace bb {

namespace detail {

constexpr uint64_t file1_mask = 0x0101010101010101ULL;
constexpr uint64_t file8_mask = 0x8080808080808080ULL;

} // namespace detail

constexpr uint64_t set(const Square sq) {
    return 0x1ULL << sq;
}

constexpr uint64_t clear(const Square sq) {
    return ~(0x1ULL << sq);
}

template <typename... Squares>
constexpr uint64_t set(const Squares... sqs) {
    return (set(sqs) | ...);
}

constexpr uint64_t file(const File file) {
    return detail::file1_mask << file;
}

constexpr uint64_t rank(const Rank rank) {
    return 0xFFULL << (rank * 8);
}

constexpr int count(const uint64_t bitboard) {
    return __builtin_popcountll(bitboard);
}

constexpr uint64_t is_many(const uint64_t bitboard) {
    return bitboard & (bitboard - 1);
}

constexpr Square lsb(const uint64_t bitboard) {
    return Square(__builtin_ctzll(bitboard));
}

constexpr Square msb(const uint64_t bitboard) {
    return Square(63 - __builtin_clzll(bitboard));
}

template <Color C>
constexpr Square select(const uint64_t bitboard) {
    return (C == WHITE) ? msb(bitboard) : lsb(bitboard);
}

constexpr Square lsb_pop(uint64_t& bitboard) {
    auto sq   = lsb(bitboard);
    bitboard &= clear(sq);
    return sq;
}

constexpr Square msb_pop(uint64_t& bitboard) {
    auto sq   = msb(bitboard);
    bitboard &= clear(sq);
    return sq;
}

template <Color C>
constexpr Square pop(uint64_t& bitboard) {
    return (C == WHITE) ? msb_pop(bitboard) : lsb_pop(bitboard);
}

template <Color C, typename Func>
inline void scan(uint64_t& bitboard, Func action) {
    while (bitboard) {
        auto sq = bb::pop<C>(bitboard);
        action(sq);
    }
}

constexpr uint64_t fill_north(uint64_t bitboard) {
    bitboard |= (bitboard << 8);
    bitboard |= (bitboard << 16);
    bitboard |= (bitboard << 32);
    return bitboard;
}

constexpr uint64_t fill_south(uint64_t bitboard) {
    bitboard |= (bitboard >> 8);
    bitboard |= (bitboard >> 16);
    bitboard |= (bitboard >> 32);
    return bitboard;
}

constexpr uint64_t fill(uint64_t bitboard) {
    return fill_north(bitboard) | fill_south(bitboard);
}

constexpr uint64_t shift_south(uint64_t bitboard) {
    return bitboard >> 8;
}

constexpr uint64_t shift_north(uint64_t bitboard) {
    return bitboard << 8;
}

constexpr uint64_t shift_east(uint64_t bitboard) {
    return (bitboard << 1) & ~detail::file1_mask;
}

constexpr uint64_t shift_west(uint64_t bitboard) {
    return (bitboard >> 1) & ~detail::file8_mask;
}

constexpr uint64_t span_north(uint64_t bitboard) {
    return shift_north(fill_north(bitboard));
}

constexpr uint64_t span_south(uint64_t bitboard) {
    return shift_south(fill_south(bitboard));
}

template <Color C>
constexpr uint64_t span_front(uint64_t bitboard) {
    return (C == WHITE) ? span_north(bitboard) : span_south(bitboard);
}

template <Color C>
constexpr uint64_t span_back(uint64_t bitboard) {
    return (C == WHITE) ? span_south(bitboard) : span_north(bitboard);
}

template <Color C>
constexpr uint64_t attack_span(uint64_t bitboard) {
    uint64_t frontspan = span_front<C>(bitboard);
    return shift_west(frontspan) | shift_east(frontspan);
}

template <Color C>
constexpr uint64_t full_span(uint64_t bitboard) {
    uint64_t frontspan = span_front<C>(bitboard);
    return shift_west(frontspan) | shift_east(frontspan) | frontspan;
}

} // namespace bb
