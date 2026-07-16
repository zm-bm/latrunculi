#pragma once

#include <array>
#include <cstddef>

#include "core/attacks_magic.hpp"
#include "core/bitboard.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"

namespace attacks::tables {

template <std::size_t N>
constexpr std::array<Bitboard, N_SQUARES> make_move_table(const int (&offsets)[N][2]) {
    std::array<Bitboard, N_SQUARES> table = {};

    for (int sq = A1; sq < N_SQUARES; ++sq) {
        const Rank rank = square::rank_of(Square(sq));
        const File file = square::file_of(Square(sq));

        Bitboard mask = 0;
        for (const auto& offset : offsets) {
            const Rank to_rank = rank + offset[0];
            const File to_file = file + offset[1];
            if (0 <= to_file && to_file < 8 && 0 <= to_rank && to_rank < 8)
                bb::add(mask, square::make(to_file, to_rank));
        }

        table[sq] = mask;
    }

    return table;
}

constexpr std::array<Bitboard, N_SQUARES> knight_moves = make_move_table({
    {+2, -1},
    {+1, -2},
    {+1, +2},
    {+2, +1},
    {-2, -1},
    {-1, -2},
    {-1, +2},
    {-2, +1},
});

constexpr std::array<Bitboard, N_SQUARES> king_moves =
    make_move_table({{+1, -1}, {+1, 0}, {+1, +1}, {0, -1}, {0, +1}, {-1, -1}, {-1, 0}, {-1, +1}});

} // namespace attacks::tables

namespace attacks {

// Initializes magic slider attack tables. Call once before using bishop, rook,
// or queen attacks. Pawn, knight, and king attack tables are constexpr and do
// not depend on initialization.
inline void init() {
    magic::init();
}

template <PieceType p>
constexpr Bitboard piece_moves(Square sq, Bitboard occupancy = 0) {
    switch (p) {
    case KNIGHT: return tables::knight_moves[sq];
    case BISHOP: return magic::bishop_moves(sq, occupancy);
    case ROOK:   return magic::rook_moves(sq, occupancy);
    case QUEEN:  return magic::queen_moves(sq, occupancy);
    case KING:   return tables::king_moves[sq];
    default:     return 0;
    }
}

constexpr Bitboard piece_moves(Square sq, PieceType p, Bitboard occupancy) {
    switch (p) {
    case KNIGHT: return tables::knight_moves[sq];
    case BISHOP: return magic::bishop_moves(sq, occupancy);
    case ROOK:   return magic::rook_moves(sq, occupancy);
    case QUEEN:  return magic::queen_moves(sq, occupancy);
    case KING:   return tables::king_moves[sq];
    default:     return 0;
    }
}

template <int Delta, Color C>
constexpr Bitboard pawn_moves(Bitboard pawns) {
    if constexpr (Delta == pawn_delta::left || Delta == pawn_delta::right) {
        constexpr File edge  = ((Delta == pawn_delta::left) ^ (C == BLACK)) ? FILE1 : FILE8;
        pawns               &= ~bb::file(edge);
    }

    return (C == WHITE) ? pawns << Delta : pawns >> Delta;
}

template <int Delta>
constexpr Bitboard pawn_moves(Bitboard pawns, Color c) {
    return (c == WHITE) ? pawn_moves<Delta, WHITE>(pawns) : pawn_moves<Delta, BLACK>(pawns);
};

template <Color C>
constexpr Bitboard pawn_attacks(Bitboard pawns) {
    return pawn_moves<pawn_delta::left, C>(pawns) | pawn_moves<pawn_delta::right, C>(pawns);
}

template <Color C>
constexpr Bitboard pawn_attacks(Square sq) {
    return pawn_attacks<C>(bb::set(sq));
}

constexpr Bitboard pawn_attacks(Bitboard pawns, Color c) {
    return pawn_moves<pawn_delta::left>(pawns, c) | pawn_moves<pawn_delta::right>(pawns, c);
}

constexpr Bitboard pawn_attacks(Square sq, Color c) {
    return pawn_attacks(bb::set(sq), c);
}

} // namespace attacks
