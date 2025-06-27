#pragma once

#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// -----------------
// Type aliases
// -----------------

using U64 = uint64_t;
using U32 = uint32_t;
using U16 = uint16_t;
using U8  = uint8_t;
using I64 = int64_t;
using I32 = int32_t;
using I16 = int16_t;
using I8  = int8_t;

using BBTable      = std::array<U64, 64>;
using BBMatrix     = std::array<std::array<U64, 64>, 64>;
using Clock        = std::chrono::high_resolution_clock;
using TimePoint    = std::chrono::high_resolution_clock::time_point;
using Milliseconds = std::chrono::milliseconds;
using CastleRights = std::bitset<4>;

// -----------------
// Enums + constants
// -----------------

enum class Rank : I8 { R1, R2, R3, R4, R5, R6, R7, R8 };
enum class File : I8 { F1, F2, F3, F4, F5, F6, F7, F8 };

constexpr size_t N_SQUARES = 64;
enum Square : I8 {
    // clang-format off
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    INVALID = 64
    // clang-format on
};

constexpr size_t N_COLORS = 2;
enum Color : U8 { BLACK, WHITE };

constexpr size_t N_PIECES = 7;
enum class PieceType : U8 {
    // clang-format off
    None = 0, All = 0,
    Pawn = 1, Knight, Bishop, Rook, Queen, King
    // clang-format on
};

enum class Piece : U8 {
    // clang-format off
    None = 0,
    BPawn = 1, BKnight, BBishop, BRook, BQueen, BKing,
    WPawn = 9, WKnight, WBishop, WRook, WQueen, WKing,
    // clang-format on
};

enum class MoveType { Normal, Promotion, EnPassant, Castle };
enum class MoveGenMode { All, Captures, Evasions, Quiets };
enum class PawnMove { Left = 7, Push = 8, Right = 9, Double = 16 };
enum class NodeType { Root, PV, NonPV };
enum class Castle { KingSide, QueenSide };
enum class Phase { MidGame, EndGame };

// -----------------
// Conversion helpers
// -----------------

template <typename E>
constexpr size_t idx(E e) noexcept {
    return static_cast<size_t>(std::to_underlying(e));
}

constexpr Color operator~(Color c) { return static_cast<Color>(c ^ WHITE); }

constexpr Rank rankOf(const Square square) { return static_cast<Rank>(square >> 3); }
constexpr File fileOf(const Square square) { return static_cast<File>(square & 7); }

constexpr Rank relativeRank(const Square square, const Color color) {
    return Rank(idx(rankOf(square)) ^ (~color * 7));
}

constexpr Color pieceColorOf(const Piece p) { return static_cast<Color>(idx(p) >> 3); }
constexpr PieceType pieceTypeOf(const Piece p) { return static_cast<PieceType>(idx(p) & 0x7); }

constexpr Piece makePiece(const Color c, const PieceType p) {
    return static_cast<Piece>((c << 3) | idx(p));
}

constexpr Square makeSquare(const File file, const Rank rank) {
    return static_cast<Square>((idx(rank) << 3) + idx(file));
}

inline Square makeSquare(const std::string& square) {
    auto file = static_cast<File>(square[0] - 'a');
    auto rank = static_cast<Rank>(square[1] - '1');
    return makeSquare(file, rank);
}

// -----------------
// String/char helpers
// -----------------

constexpr char PIECE_CHARS[] = {
    ' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ', ' ', 'P', 'N', 'B', 'R', 'Q', 'K', ' '};

constexpr char toChar(Piece p) { return PIECE_CHARS[idx(p)]; }
constexpr char toChar(PieceType pt) { return PIECE_CHARS[idx(pt)]; }

constexpr char toChar(Rank rank) { return static_cast<char>('1' + int(rank)); }
constexpr char toChar(File file) { return static_cast<char>('a' + int(file)); }

inline std::string toString(Square sq) {
    return std::string{toChar(File(sq & 7))} + toChar(Rank(sq >> 3));
}

inline std::ostream& operator<<(std::ostream& os, Color color) {
    os << (color == WHITE ? "white" : "black");
    return os;
}

inline std::ostream& operator<<(std::ostream& os, File file) {
    os << toChar(file);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Rank rank) {
    os << toChar(rank);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Square sq) {
    os << toString(sq);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Piece piece) {
    os << toChar(piece);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, PieceType pieceType) {
    os << toChar(pieceType);
    return os;
}

// -----------------
// Move helpers
// -----------------

template <Color c, PawnMove p, bool forward>
inline Square pawnMove(const Square sq) {
    return (forward == (c == WHITE)) ? Square(sq + static_cast<int>(p))
                                     : Square(sq - static_cast<int>(p));
}

template <PawnMove p, bool forward>
inline Square pawnMove(const Square sq, const Color c) {
    return (c == WHITE) ? pawnMove<WHITE, p, forward>(sq) : pawnMove<BLACK, p, forward>(sq);
}

// -----------------
// Operators
// -----------------

constexpr Square operator+(Square sq1, int sq2) { return Square(int(sq1) + sq2); }
constexpr Square operator-(Square sq1, int sq2) { return Square(int(sq1) - sq2); }
constexpr Square& operator++(Square& sq) { return sq = Square(int(sq) + 1); }
constexpr Square& operator--(Square& sq) { return sq = Square(int(sq) - 1); }

constexpr Rank operator+(Rank r1, int r2) { return Rank(int(r1) + r2); }
constexpr Rank operator-(Rank r1, int r2) { return Rank(int(r1) - r2); }
constexpr Rank& operator++(Rank& r) { return r = r + 1; }
constexpr Rank& operator--(Rank& r) { return r = r - 1; }

constexpr File operator+(File f1, int f2) { return File(int(f1) + f2); }
constexpr File operator-(File f1, int f2) { return File(int(f1) - f2); }
constexpr File& operator++(File& f) { return f = f + 1; }
constexpr File& operator--(File& f) { return f = f - 1; }
