#pragma once

#include <cassert>
#include <cstdint>

#include "core/types.hpp"

// Piece type occupies the low three bits of a Piece encoding.
enum PieceType : std::uint8_t {
    NO_PIECETYPE = 0,
    PAWN         = 1,
    KNIGHT       = 2,
    BISHOP       = 3,
    ROOK         = 4,
    QUEEN        = 5,
    KING         = 6,
    N_PIECETYPES = 7,
};

// Colored piece encoding: color in bit 3, piece type in bits 0-2.
enum Piece : std::uint8_t {
    NO_PIECE = 0,
    B_PAWN   = 1,
    B_KNIGHT = 2,
    B_BISHOP = 3,
    B_ROOK   = 4,
    B_QUEEN  = 5,
    B_KING   = 6,
    W_PAWN   = 9,
    W_KNIGHT = 10,
    W_BISHOP = 11,
    W_ROOK   = 12,
    W_QUEEN  = 13,
    W_KING   = 14,
};

// Number of concrete piece-type slots, excluding NO_PIECETYPE.
inline constexpr int piece_slots = static_cast<int>(KING) - static_cast<int>(PAWN) + 1;

// Aggregate slot used by piece-count/material arrays.
inline constexpr int all_pieces_slot = static_cast<int>(NO_PIECETYPE);

// Packs color and piece type into the shared Piece encoding.
constexpr Piece make_piece(const Color c, const PieceType pt) {
    return static_cast<Piece>((c << 3) | pt);
}

// Extracts color from a colored Piece encoding.
constexpr Color color_of(const Piece p) {
    return static_cast<Color>(p >> 3);
}

// Extracts piece type from a Piece encoding.
constexpr PieceType type_of(const Piece p) {
    return static_cast<PieceType>(p & 0x7);
}

// True for concrete piece types, excluding sentinels.
constexpr bool is_piece_type(PieceType piece) {
    return piece >= PAWN && piece <= KING;
}

// True for legal promotion result types.
constexpr bool valid_promotion_piece(PieceType piece) {
    return piece >= KNIGHT && piece <= QUEEN;
}

// Zero-based material/count index for a concrete piece type.
constexpr int piece_slot(PieceType piece) {
    assert(is_piece_type(piece));
    return static_cast<int>(piece) - static_cast<int>(PAWN);
}
