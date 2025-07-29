#pragma once

#include "defs.hpp"

constexpr Square make_square(const File file, const Rank rank) {
    return static_cast<Square>((rank << 3) + file);
}

inline Square make_square(const std::string& square) {
    auto file = static_cast<File>(square[0] - 'a');
    auto rank = static_cast<Rank>(square[1] - '1');
    return make_square(file, rank);
}

constexpr Square relative_square(const Square sq, const Color c) {
    return static_cast<Square>(sq ^ ((c - 1) & 63));
}

constexpr Rank rank_of(const Square square) {
    return static_cast<Rank>(square >> 3);
}
constexpr File file_of(const Square square) {
    return static_cast<File>(square & 7);
}

constexpr Rank relative_rank(const Square square, const Color color) {
    return static_cast<Rank>(rank_of(square) ^ (~color * 7));
}

constexpr Piece make_piece(const Color c, const PieceType pt) {
    return static_cast<Piece>((c << 3) | pt);
}

constexpr Color color_of(const Piece p) {
    return static_cast<Color>(p >> 3);
}

constexpr PieceType type_of(const Piece p) {
    return static_cast<PieceType>(p & 0x7);
}
