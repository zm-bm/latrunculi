#ifndef LATRUNCULI_TYPES_H
#define LATRUNCULI_TYPES_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using U64 = uint64_t;
using U32 = uint32_t;
using U16 = uint16_t;
using U8 = uint8_t;
using I64 = int64_t;
using I32 = int32_t;
using I16 = int16_t;
using I8 = int8_t;

enum Color : U8 { BLACK, WHITE, NCOLORS = 2 };

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
  NSQUARES = 64
};
// clang-format on

// clang-format off
enum Piece : U8 {
  NO_PIECE = 0,
  B_PAWN = 1, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
  W_PAWN = 9, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
};

enum PieceType : U8 {
  NO_PIECE_TYPE = 0, ALL_PIECE_TYPES = 0,
  PAWN = 1, KNIGHT, BISHOP, ROOK, QUEEN, KING,
  N_PIECE_TYPES = 6
};
// clang-format on

struct PieceSquare {
    Color color;
    PieceType role;
    Square square;
};

enum File : I8 { FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8, NFILES };
enum Rank : I8 { RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8, NRANKS };

enum CastleRights : U8 {
    NO_CASTLE = 0x0,
    BLACK_OOO = 0x1,
    BLACK_OO = 0x2,
    WHITE_OOO = 0x4,
    WHITE_OO = 0x8,
    BLACK_CASTLE = BLACK_OO | BLACK_OOO,
    WHITE_CASTLE = WHITE_OO | WHITE_OOO,
    ALL_CASTLE = BLACK_CASTLE | WHITE_CASTLE
};

enum CastleDirection { KINGSIDE, QUEENSIDE };

enum MoveType : U8 { NORMAL, PROMOTION, ENPASSANT, CASTLE };

enum class PawnMove { LEFT = 7, PUSH = 8, RIGHT = 9, DOUBLE = 16 };

enum Score {
    DRAWSCORE = 0,
    PAWNSCORE = 100,
    KNIGHTSCORE = 320,
    BISHOPSCORE = 330,
    ROOKSCORE = 500,
    QUEENSCORE = 900,
    TOTALPHASE =
        2 * QUEENSCORE + 4 * ROOKSCORE + 4 * BISHOPSCORE + 4 * KNIGHTSCORE + 16 * PAWNSCORE,
    KINGSCORE = 20000,
    MATESCORE = 32000,
};

enum Phase {
    MIDGAME = 0,
    ENDGAME = 1,
};

enum NodeType : U8 {
    TT_NONE,
    TT_EXACT,
    TT_ALPHA,
    TT_BETA,
};

inline Color operator~(Color c) { return Color(c ^ WHITE); }

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
    constexpr char pieceToChar[16] =
        {' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ', ' ', 'P', 'N', 'B', 'R', 'Q', 'K', ' '};
    os << pieceToChar[p];
    return os;
}

// Enable arithmetic and bitwise operators
#define ENABLE_OPERATORS(T)                                                   \
    inline constexpr T operator+(T d1, T d2) { return T(int(d1) + int(d2)); } \
    inline constexpr T operator+(T d1, int d2) { return T(int(d1) + d2); }    \
    inline constexpr T operator-(T d1, T d2) { return T(int(d1) - int(d2)); } \
    inline constexpr T operator-(T d1, int d2) { return T(int(d1) - d2); }    \
    inline constexpr T operator*(T d, int i) { return T(int(d) * i); }        \
    inline constexpr T operator*(int i, T d) { return T(i * int(d)); }        \
    inline constexpr T operator/(T d1, T d2) { return T(int(d1) / int(d2)); } \
    inline constexpr T operator/(T d, int i) { return T(int(d) / i); }        \
    inline constexpr T& operator+=(T& d1, T d2) { return d1 = d1 + d2; }      \
    inline constexpr T& operator-=(T& d1, T d2) { return d1 = d1 - d2; }      \
    inline constexpr T& operator*=(T& d, int i) { return d = T(int(d) * i); } \
    inline constexpr T& operator/=(T& d, int i) { return d = T(int(d) / i); } \
    inline constexpr T& operator++(T& d, int) { return d = T(int(d) + 1); }   \
    inline constexpr T& operator--(T& d, int) { return d = T(int(d) - 1); }   \
    inline constexpr T& operator&=(T& d1, T d2) { return d1 = T(d1 & d2); }   \
    inline constexpr T& operator|=(T& d1, T d2) { return d1 = T(d1 | d2); }

ENABLE_OPERATORS(Square)
ENABLE_OPERATORS(File)
ENABLE_OPERATORS(Rank)
ENABLE_OPERATORS(CastleRights)
ENABLE_OPERATORS(Score)

#undef ENABLE_OPERATORS

#endif