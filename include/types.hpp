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

enum Piece : U8 {
  NO_PIECE = 0,
  B_PAWN = 1, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
  W_PAWN = 9, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
};

enum PieceRole : U8 {
  NO_PIECE_ROLE = 0, ALL_PIECE_ROLES = 0,
  PAWN = 1, KNIGHT, BISHOP, ROOK, QUEEN, KING,
  N_PIECES = 6
};
// clang-format on

enum File : I8 { FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8 };
enum Rank : I8 { RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8 };

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

enum MoveType : U8 { NORMAL, PROMOTION, ENPASSANT, CASTLE, NULLMOVE };

enum MoveGenType { EVASIONS, CAPTURES, QUIETS };

enum class PawnMove { LEFT = 7, PUSH = 8, RIGHT = 9, DOUBLE = 16 };

enum Score {
  DRAWSCORE = 0,
  PAWNSCORE = 100,
  KNIGHTSCORE = 320,
  BISHOPSCORE = 330,
  ROOKSCORE = 500,
  QUEENSCORE = 900,
  TOTALPHASE = 2 * QUEENSCORE + 4 * ROOKSCORE + 4 * BISHOPSCORE +
               4 * KNIGHTSCORE + 16 * PAWNSCORE,
  KINGSCORE = 20000,
  MATESCORE = 32000,
};

enum Phase {
  OPENING = 0,
  ENDGAME = 1,
};

enum NodeType : U8 {
  TT_NONE,
  TT_EXACT,
  TT_ALPHA,
  TT_BETA,
};

namespace Types {

constexpr Square getSquare(const File file, const Rank rank) {
  return Square(rank * 8 + file);
}

inline Square getSquareFromStr(const std::string& square) {
  auto file = File((int)square[0] - 'a');
  auto rank = Rank(((int)square[1] - '1'));

  return getSquare(file, rank);
}

constexpr Rank getRank(const Square square) { return Rank(square >> 3); }

constexpr File getFile(const Square square) { return File(square & 7); }

constexpr bool validRank(const Rank rank) {
  return (RANK1 <= rank) && (rank <= RANK8);
}

constexpr bool validFile(const File file) {
  return (FILE1 <= file) && (file <= FILE8);
}

constexpr Piece makePiece(const Color c, const PieceRole p) {
  return Piece((c << 3) | p);
}

constexpr PieceRole getPieceRole(const Piece p) { return PieceRole(p & 0x7); }

constexpr Color getPieceColor(const Piece p) { return Color(p >> 3); }

template <Color c, PawnMove p, bool forward>
inline Square pawnMove(const Square sq) {
  return (forward == (c == WHITE)) ? Square(sq + static_cast<int>(p))
                                   : Square(sq - static_cast<int>(p));
}

template <PawnMove p, bool forward>
inline Square pawnMove(const Square sq, const Color c) {
  return (c == WHITE) ? pawnMove<WHITE, p, forward>(sq)
                      : pawnMove<BLACK, p, forward>(sq);
}

const char PieceChar[16] = {' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ',
                            ' ', 'P', 'N', 'B', 'R', 'Q', 'K', ' '};

}  // namespace Types

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
  os << Types::getFile(sq) << Types::getRank(sq);
  return os;
}

inline std::ostream& operator<<(std::ostream& os, Piece p) {
  os << Types::PieceChar[p];
  return os;
}

// Enable arithmetic and bitwise operators
#define ENABLE_OPERATORS(T)                                                 \
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