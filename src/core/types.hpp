#pragma once

#include <cstdint>

enum Color : uint8_t { BLACK, WHITE, N_COLORS };

enum Rank : int8_t { RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8 };

enum File : int8_t { FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8 };

enum Square : int8_t {
    // clang-format off
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    // clang-format on
    INVALID,
    N_SQUARES = 64
};

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

enum Direction {
    NORTH_EAST = 9,
    NORTH      = 8,
    NORTH_WEST = 7,
    EAST       = 1,
    WEST       = -1,
    SOUTH_EAST = -7,
    SOUTH      = -8,
    SOUTH_WEST = -9,
};

enum PawnMove {
    PAWN_LEFT  = 7,
    PAWN_PUSH  = 8,
    PAWN_RIGHT = 9,
    PAWN_PUSH2 = 16,
};

enum MoveType { BASIC_MOVE, MOVE_PROM, MOVE_EP, MOVE_CASTLE };

constexpr Color operator~(Color c) {
    return static_cast<Color>(c ^ WHITE);
}

constexpr Square operator+(Square sq1, int sq2) {
    return Square(int(sq1) + sq2);
}

constexpr Square operator-(Square sq1, int sq2) {
    return Square(int(sq1) - sq2);
}

constexpr Square& operator++(Square& sq) {
    return sq = Square(int(sq) + 1);
}

constexpr Square& operator--(Square& sq) {
    return sq = Square(int(sq) - 1);
}

constexpr Rank operator+(Rank r1, int r2) {
    return Rank(int(r1) + r2);
}

constexpr Rank operator-(Rank r1, int r2) {
    return Rank(int(r1) - r2);
}

constexpr Rank& operator++(Rank& r) {
    return r = r + 1;
}

constexpr Rank& operator--(Rank& r) {
    return r = r - 1;
}

constexpr File operator+(File f1, int f2) {
    return File(int(f1) + f2);
}

constexpr File operator-(File f1, int f2) {
    return File(int(f1) - f2);
}

constexpr File& operator++(File& f) {
    return f = f + 1;
}

constexpr File& operator--(File& f) {
    return f = f - 1;
}
