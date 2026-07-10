#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

#include "core/piece.hpp"
#include "core/square.hpp"

namespace notation_detail {
inline constexpr char pieces[] = {
    ' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ', ' ', 'P', 'N', 'B', 'R', 'Q', 'K', ' '};
}

constexpr char to_char(Piece p) {
    return notation_detail::pieces[p];
}

constexpr char to_char(PieceType pt) {
    return notation_detail::pieces[pt];
}

constexpr char to_char(Rank rank) {
    return static_cast<char>('1' + int(rank));
}

constexpr char to_char(File file) {
    return static_cast<char>('a' + int(file));
}

inline std::string to_string(Square sq) {
    return std::string{to_char(square::file_of(sq))} + to_char(square::rank_of(sq));
}

constexpr Square parse_square(std::string_view text) {
    auto file = static_cast<File>(text[0] - 'a');
    auto rank = static_cast<Rank>(text[1] - '1');
    return square::make(file, rank);
}

inline std::ostream& operator<<(std::ostream& os, Color color) {
    os << (color == WHITE ? "white" : "black");
    return os;
}

inline std::ostream& operator<<(std::ostream& os, File file) {
    os << to_char(file);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Rank rank) {
    os << to_char(rank);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Square sq) {
    os << to_string(sq);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Piece piece) {
    os << to_char(piece);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, PieceType pieceType) {
    os << to_char(pieceType);
    return os;
}

// Stream wrapper for printing a raw uint64_t as an 8x8 bitboard during debugging.
struct BitboardDebug {
    uint64_t bitboard;
};

BitboardDebug debug_bitboard(uint64_t bitboard);
void          print_bitboard(uint64_t bitboard);
std::ostream& operator<<(std::ostream& os, BitboardDebug debug);
