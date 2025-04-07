#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using U64 = uint64_t;
using U32 = uint32_t;
using U16 = uint16_t;
using U8  = uint8_t;
using I64 = int64_t;
using I32 = int32_t;
using I16 = int16_t;
using I8  = int8_t;

enum class NodeType { Root, PV, NonPV };

enum Color : U8 { BLACK, WHITE, N_COLORS = 2 };

// clang-format off
enum Square : U8 {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8,
  INVALID = 64,
  N_SQUARES = 64
};

enum PawnMove : U8 { LEFT = 7, PUSH = 8, RIGHT = 9, DOUBLE = 16 };

enum class Piece : U8 {
  NONE = 0,
  B_PAWN = 1, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
  W_PAWN = 9, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
};

enum PieceType : U8 {
  NO_PIECE_TYPE = 0, ALL_PIECES = 0,
  PAWN = 1, KNIGHT, BISHOP, ROOK, QUEEN, KING,
  N_PIECES = 7
};
// clang-format on

enum File : I8 { FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8, N_FILES };
enum Rank : I8 { RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8, N_RANKS };

enum CastleRights {
    NO_CASTLE    = 0,
    BLACK_OOO    = 1,
    BLACK_OO     = 1 << 1,
    WHITE_OOO    = 1 << 2,
    WHITE_OO     = 1 << 3,
    BLACK_CASTLE = BLACK_OO | BLACK_OOO,
    WHITE_CASTLE = WHITE_OO | WHITE_OOO,
    ALL_CASTLE   = BLACK_CASTLE | WHITE_CASTLE
};

enum CastleDirection { KINGSIDE, QUEENSIDE, N_CASTLES };

enum MoveType { NORMAL, PROMOTION, ENPASSANT, CASTLE };

enum Phase { MIDGAME, ENDGAME, N_PHASES };

enum class GenType { All, Captures, Evasions, Quiets };

struct PieceSquare {
    Color color;
    PieceType type;
    Square square;
};

// Operators

constexpr Color operator~(Color c) { return Color(c ^ WHITE); }

inline std::ostream& operator<<(std::ostream& os, Color color) {
    os << (color == WHITE ? "white" : "black");
    return os;
}

inline std::ostream& operator<<(std::ostream& os, File file) {
    os << static_cast<char>('a' + file);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Rank rank) {
    os << static_cast<char>('1' + rank);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Square sq) {
    os << File(sq & 7) << Rank(sq >> 3);
    return os;
}

inline std::ostream& operator<<(std::ostream& os, Piece p) {
    static const char pieces[16] =
        {' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ', ' ', 'P', 'N', 'B', 'R', 'Q', 'K', ' '};
    os << pieces[static_cast<int>(p)];
    return os;
}

inline std::ostream& operator<<(std::ostream& os, PieceType p) {
    static constexpr char pieces[] =
        {' ', 'p', 'n', 'b', 'r', 'q', 'k', ' '};
    os << pieces[static_cast<int>(p)];
    return os;
}

// Helpers

constexpr Square makeSquare(const File file, const Rank rank) {
    return static_cast<Square>((rank << 3) + file);
}

inline Square makeSquare(const std::string& square) {
    auto file = File(static_cast<int>(square[0]) - 'a');
    auto rank = Rank(static_cast<int>(square[1]) - '1');
    return makeSquare(file, rank);
}

constexpr Rank rankOf(const Square square) { return Rank(square >> 3); }

constexpr Rank relativeRank(const Rank rank, const Color color) {
    return Rank(rank ^ (~color * 7));
}
constexpr Rank relativeRank(const Square square, const Color color) {
    return Rank(rankOf(square) ^ (~color * 7));
}

constexpr File fileOf(const Square square) { return File(square & 7); }

constexpr Piece makePiece(const Color c, const PieceType p) {
    // create piece from color and piece type
    return static_cast<Piece>((c << 3) | static_cast<int>(p));
}

constexpr PieceType pieceTypeOf(const Piece p) {
    // get the piece type from a piece
    return PieceType(static_cast<int>(p) & 0x7);
}

constexpr Color pieceColorOf(const Piece p) {
    // get the color of a piece
    return Color(static_cast<int>(p) >> 3);
}

template <Color c, PawnMove p, bool forward>
inline Square pawnMove(const Square sq) {
    return (forward == (c == WHITE)) ? Square(sq + static_cast<int>(p))
                                     : Square(sq - static_cast<int>(p));
}

template <PawnMove p, bool forward>
inline Square pawnMove(const Square sq, const Color c) {
    return (c == WHITE) ? pawnMove<WHITE, p, forward>(sq) : pawnMove<BLACK, p, forward>(sq);
}

// Enable arithmetic and bitwise operators
#define ENABLE_OPERATORS(T)                                            \
    constexpr T operator+(T d1, T d2) { return T(int(d1) + int(d2)); } \
    constexpr T operator+(T d1, int d2) { return T(int(d1) + d2); }    \
    constexpr T operator-(T d1, T d2) { return T(int(d1) - int(d2)); } \
    constexpr T operator-(T d1, int d2) { return T(int(d1) - d2); }    \
    constexpr T operator*(T d, int i) { return T(int(d) * i); }        \
    constexpr T operator*(int i, T d) { return T(i * int(d)); }        \
    constexpr T operator/(T d1, T d2) { return T(int(d1) / int(d2)); } \
    constexpr T operator/(T d, int i) { return T(int(d) / i); }        \
    constexpr T operator~(T d) { return T(~int(d)); }                  \
    constexpr T& operator+=(T& d1, T d2) { return d1 = d1 + d2; }      \
    constexpr T& operator-=(T& d1, T d2) { return d1 = d1 - d2; }      \
    constexpr T& operator*=(T& d, int i) { return d = T(int(d) * i); } \
    constexpr T& operator/=(T& d, int i) { return d = T(int(d) / i); } \
    constexpr T& operator++(T& d, int) { return d = T(int(d) + 1); }   \
    constexpr T& operator--(T& d, int) { return d = T(int(d) - 1); }   \
    constexpr T& operator&=(T& d1, T d2) { return d1 = T(d1 & d2); }   \
    constexpr T& operator|=(T& d1, T d2) { return d1 = T(d1 | d2); }

ENABLE_OPERATORS(Square)
ENABLE_OPERATORS(File)
ENABLE_OPERATORS(Rank)
ENABLE_OPERATORS(CastleRights)

#undef ENABLE_OPERATORS
