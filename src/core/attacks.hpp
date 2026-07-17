#pragma once

#include <array>
#include <cstddef>

#include "core/attacks_magic.hpp"
#include "core/bitboard.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"

namespace attacks::tables {

// Builds leaper attack tables from file/rank offsets.
template <std::size_t N>
constexpr std::array<Bitboard, N_SQUARES> make_move_table(const int (&offsets)[N][2]) {
    std::array<Bitboard, N_SQUARES> table = {};

    for (Square sq = A1; sq < N_SQUARES; ++sq) {
        const Rank rank = square::rank_of(sq);
        const File file = square::file_of(sq);

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

// Precomputed knight attacks by source square.
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

// Precomputed king attacks by source square.
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

// Compile-time dispatch for piece attacks when the piece type is known.
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

// Runtime dispatch for piece attacks when the piece type is data.
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

// Color-relative pawn shift; diagonal shifts trim edge-file wraparound.
template <int Delta, Color C>
constexpr Bitboard pawn_shift(Bitboard pawns) {
    if constexpr (Delta == pawn_delta::left || Delta == pawn_delta::right) {
        constexpr File edge = ((Delta == pawn_delta::left) ^ (C == BLACK)) ? FILE1 : FILE8;

        pawns &= ~bb::file(edge);
    }

    return (C == WHITE) ? pawns << Delta : pawns >> Delta;
}

// Runtime-color wrapper for a pawn shift.
template <int Delta>
constexpr Bitboard pawn_shift(Bitboard pawns, Color c) {
    return (c == WHITE) ? pawn_shift<Delta, WHITE>(pawns) : pawn_shift<Delta, BLACK>(pawns);
};

// Pawn attack mask for all supplied pawns.
template <Color C>
constexpr Bitboard pawn_attacks(Bitboard pawns) {
    return pawn_shift<pawn_delta::left, C>(pawns) | pawn_shift<pawn_delta::right, C>(pawns);
}

// Pawn attack mask for one source square.
template <Color C>
constexpr Bitboard pawn_attacks(Square sq) {
    return pawn_attacks<C>(bb::set(sq));
}

// Runtime-color pawn attack mask for all supplied pawns.
constexpr Bitboard pawn_attacks(Bitboard pawns, Color c) {
    return pawn_shift<pawn_delta::left>(pawns, c) | pawn_shift<pawn_delta::right>(pawns, c);
}

// Runtime-color pawn attack mask for one source square.
constexpr Bitboard pawn_attacks(Square sq, Color c) {
    return pawn_attacks(bb::set(sq), c);
}

} // namespace attacks
