#pragma once

#include <cassert>
#include <ostream>
#include <string>
#include <string_view>

#include "core/piece.hpp"
#include "core/square.hpp"

namespace {

// Piece-character table indexed by Piece or PieceType encoding.
inline constexpr char piece_chars[] = {
    ' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ', ' ', 'P', 'N', 'B', 'R', 'Q', 'K', ' '};

} // namespace

// Converts a valid piece encoding to its FEN/debug character.
constexpr char to_char(Piece p) {
    assert(p == NO_PIECE || (color_of(p) < N_COLORS && is_piece_type(type_of(p))));
    return piece_chars[p];
}

// Converts a valid piece type to its lowercase FEN/debug character.
constexpr char to_char(PieceType pt) {
    assert(pt == NO_PIECETYPE || is_piece_type(pt));
    return piece_chars[pt];
}

// Converts a board rank to algebraic notation.
constexpr char to_char(Rank rank) {
    assert(RANK1 <= rank && rank <= RANK8);
    return static_cast<char>('1' + int(rank));
}

// Converts a board file to algebraic notation.
constexpr char to_char(File file) {
    assert(FILE1 <= file && file <= FILE8);
    return static_cast<char>('a' + int(file));
}

// Converts a real board square to algebraic notation.
inline std::string to_string(Square sq) {
    assert(A1 <= sq && sq < INVALID);
    return std::string{to_char(square::file_of(sq))} + to_char(square::rank_of(sq));
}

// Parses a two-character algebraic square; asserts on malformed input.
constexpr Square parse_square(std::string_view text) {
    assert(text.size() == 2);
    assert('a' <= text[0] && text[0] <= 'h');
    assert('1' <= text[1] && text[1] <= '8');

    auto file = static_cast<File>(text[0] - 'a');
    auto rank = static_cast<Rank>(text[1] - '1');
    return square::make(file, rank);
}

// Writes side names for debug text.
inline std::ostream& operator<<(std::ostream& os, Color color) {
    os << (color == WHITE ? "white" : "black");
    return os;
}

// Writes an algebraic file character.
inline std::ostream& operator<<(std::ostream& os, File file) {
    os << to_char(file);
    return os;
}

// Writes an algebraic rank character.
inline std::ostream& operator<<(std::ostream& os, Rank rank) {
    os << to_char(rank);
    return os;
}

// Writes an algebraic square.
inline std::ostream& operator<<(std::ostream& os, Square sq) {
    os << to_string(sq);
    return os;
}

// Writes a piece character.
inline std::ostream& operator<<(std::ostream& os, Piece piece) {
    os << to_char(piece);
    return os;
}

// Writes a piece-type character.
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
