#pragma once

#include "core/types.hpp"

enum PieceType : uint8_t {
    NO_PIECETYPE = 0,
    ALL_PIECES   = 0,
    PAWN         = 1,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    N_PIECES,
};

enum Piece : uint8_t {
    NO_PIECE = 0,
    B_PAWN   = 1,
    B_KNIGHT,
    B_BISHOP,
    B_ROOK,
    B_QUEEN,
    B_KING,
    W_PAWN = 9,
    W_KNIGHT,
    W_BISHOP,
    W_ROOK,
    W_QUEEN,
    W_KING,
};

constexpr Piece make_piece(const Color c, const PieceType pt) {
    return static_cast<Piece>((c << 3) | pt);
}

constexpr Color color_of(const Piece p) {
    return static_cast<Color>(p >> 3);
}

constexpr PieceType type_of(const Piece p) {
    return static_cast<PieceType>(p & 0x7);
}
