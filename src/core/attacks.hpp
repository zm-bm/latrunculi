#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/attack_magic.hpp"
#include "core/bitboard.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"

enum PawnMove {
    PAWN_LEFT  = 7,
    PAWN_PUSH  = 8,
    PAWN_RIGHT = 9,
    PAWN_PUSH2 = 16,
};

namespace attacks::tables {

template <std::size_t N>
constexpr std::array<uint64_t, N_SQUARES> make_move_table(const int (&offsets)[N][2]) {
    std::array<uint64_t, N_SQUARES> table = {};

    for (int sq = A1; sq < N_SQUARES; ++sq) {
        const Rank rank = square::rank_of(Square(sq));
        const File file = square::file_of(Square(sq));

        uint64_t mask = 0;
        for (const auto& offset : offsets) {
            const Rank to_rank = rank + offset[0];
            const File to_file = file + offset[1];
            if (0 <= to_file && to_file < 8 && 0 <= to_rank && to_rank < 8)
                mask |= bb::set(square::make(to_file, to_rank));
        }

        table[sq] = mask;
    }

    return table;
}

constexpr std::array<uint64_t, N_SQUARES> knight_moves = make_move_table({
    {+2, -1},
    {+1, -2},
    {+1, +2},
    {+2, +1},
    {-2, -1},
    {-1, -2},
    {-1, +2},
    {-2, +1},
});

constexpr std::array<uint64_t, N_SQUARES> king_moves =
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
constexpr uint64_t piece_moves(Square sq, uint64_t occupancy = 0) {
    switch (p) {
    case KNIGHT: return tables::knight_moves[sq];
    case BISHOP: return magic::bishop_moves(sq, occupancy);
    case ROOK:   return magic::rook_moves(sq, occupancy);
    case QUEEN:  return magic::queen_moves(sq, occupancy);
    case KING:   return tables::king_moves[sq];
    default:     return 0;
    }
}

constexpr uint64_t piece_moves(Square sq, PieceType p, uint64_t occupancy) {
    switch (p) {
    case KNIGHT: return tables::knight_moves[sq];
    case BISHOP: return magic::bishop_moves(sq, occupancy);
    case ROOK:   return magic::rook_moves(sq, occupancy);
    case QUEEN:  return magic::queen_moves(sq, occupancy);
    case KING:   return tables::king_moves[sq];
    default:     return 0;
    }
}

template <PawnMove M, Color C>
constexpr uint64_t pawn_moves(uint64_t pawns) {
    if constexpr (M == PAWN_LEFT || M == PAWN_RIGHT) {
        constexpr File edge  = ((M == PAWN_LEFT) ^ (C == BLACK)) ? FILE1 : FILE8;
        pawns               &= ~bb::file(edge);
    }

    return (C == WHITE) ? pawns << M : pawns >> M;
}

template <PawnMove M>
constexpr uint64_t pawn_moves(uint64_t pawns, Color c) {
    return (c == WHITE) ? pawn_moves<M, WHITE>(pawns) : pawn_moves<M, BLACK>(pawns);
};

template <Color C>
constexpr uint64_t pawn_attacks(uint64_t pawns) {
    return pawn_moves<PAWN_LEFT, C>(pawns) | pawn_moves<PAWN_RIGHT, C>(pawns);
}

constexpr uint64_t pawn_attacks(uint64_t pawns, Color c) {
    return pawn_moves<PAWN_LEFT>(pawns, c) | pawn_moves<PAWN_RIGHT>(pawns, c);
}

} // namespace attacks
