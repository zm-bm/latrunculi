#pragma once

#include <cassert>
#include <ostream>
#include <string>
#include <string_view>

#include "core/piece.hpp"
#include "core/square.hpp"

namespace {

inline constexpr char piece_chars[] = {
    ' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ', ' ', 'P', 'N', 'B', 'R', 'Q', 'K', ' '};

} // namespace

constexpr char to_char(Piece p) {
    assert(p == NO_PIECE || (color_of(p) < N_COLORS && is_piece_type(type_of(p))));
    return piece_chars[p];
}

constexpr char to_char(PieceType pt) {
    assert(pt == NO_PIECETYPE || is_piece_type(pt));
    return piece_chars[pt];
}

constexpr char to_char(Rank rank) {
    assert(RANK1 <= rank && rank <= RANK8);
    return static_cast<char>('1' + int(rank));
}

constexpr char to_char(File file) {
    assert(FILE1 <= file && file <= FILE8);
    return static_cast<char>('a' + int(file));
}

inline std::string to_string(Square sq) {
    assert(A1 <= sq && sq < INVALID);
    return std::string{to_char(square::file_of(sq))} + to_char(square::rank_of(sq));
}

constexpr Square parse_square(std::string_view text) {
    assert(text.size() == 2);
    assert('a' <= text[0] && text[0] <= 'h');
    assert('1' <= text[1] && text[1] <= '8');

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

// Stream wrapper for printing a Bitboard as an 8x8 board during debugging.
struct BitboardDebug {
    Bitboard bitboard;
};

BitboardDebug debug_bitboard(Bitboard bitboard);
void          print_bitboard(Bitboard bitboard);
std::ostream& operator<<(std::ostream& os, BitboardDebug debug);
