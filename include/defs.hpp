#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#ifndef LATRUNCULI_VERSION
#define LATRUNCULI_VERSION "0.0.0"
#endif

using Clock        = std::chrono::high_resolution_clock;
using TimePoint    = std::chrono::high_resolution_clock::time_point;
using Milliseconds = std::chrono::milliseconds;

constexpr int  MAX_DEPTH       = 64;
constexpr int  MAX_MOVES       = 128;
constexpr int  DEFAULT_THREADS = 1;
constexpr int  DEFAULT_HASH    = 4;
constexpr bool SEARCH_STATS    = false;

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

enum Value : int16_t {
    DRAW_VALUE    = 0,
    INF_VALUE     = INT16_MAX,
    MATE_VALUE    = INT16_MAX - 1,
    MATE_BOUND    = MATE_VALUE - MAX_DEPTH,
    TT_MATE_BOUND = MATE_VALUE - 2 * MAX_DEPTH,

    PAWN_MG   = 100,
    KNIGHT_MG = 630,
    BISHOP_MG = 660,
    ROOK_MG   = 1000,
    QUEEN_MG  = 2000,

    PAWN_EG   = 166,
    KNIGHT_EG = 680,
    BISHOP_EG = 740,
    ROOK_EG   = 1100,
    QUEEN_EG  = 2150,
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

enum CastleSide { CASTLE_KINGSIDE, CASTLE_QUEENSIDE, N_CASTLES };

enum CastleRights : uint8_t {
    NO_CASTLE   = 0b0000,
    B_QUEENSIDE = 0b0001,
    B_KINGSIDE  = 0b0010,
    W_QUEENSIDE = 0b0100,
    W_KINGSIDE  = 0b1000,
    B_CASTLE    = B_KINGSIDE | B_QUEENSIDE,
    W_CASTLE    = W_KINGSIDE | W_QUEENSIDE,
    ALL_CASTLE  = B_CASTLE | W_CASTLE,
};

enum MoveType { BASIC_MOVE, MOVE_PROM, MOVE_EP, MOVE_CASTLE };

enum GeneratorMode { ALL_MOVES, CAPTURES, EVASIONS, QUIETS };

enum MovePriority : uint16_t {
    PRIORITY_HASH    = 1 << 15,
    PRIORITY_PV      = 1 << 14,
    PRIORITY_PROM    = 1 << 13,
    PRIORITY_CAPTURE = 1 << 12,
    PRIORITY_KILLER  = 1 << 11,
    PRIORITY_HISTORY = 1 << 10,
    PRIORITY_WEAK    = 0,
};

enum EvalTerm {
    TERM_MATERIAL,
    TERM_SQUARES,
    TERM_PAWNS,
    TERM_KNIGHTS,
    TERM_BISHOPS,
    TERM_ROOKS,
    TERM_QUEENS,
    TERM_KING,
    TERM_MOBILITY,
    TERM_THREATS,
    N_TERMS,
};

enum Phase { MIDGAME, ENDGAME, N_PHASES };

enum NodeType { ROOT, PV, NON_PV };

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

constexpr CastleRights operator~(CastleRights cr) {
    return static_cast<CastleRights>(~uint8_t(cr));
}

constexpr CastleRights operator|(CastleRights lhs, CastleRights rhs) {
    return CastleRights(uint8_t(lhs) | uint8_t(rhs));
}

constexpr CastleRights operator&(CastleRights lhs, CastleRights rhs) {
    return CastleRights(uint8_t(lhs) & uint8_t(rhs));
}

constexpr CastleRights& operator|=(CastleRights& lhs, CastleRights rhs) {
    lhs = lhs | rhs;
    return lhs;
}

constexpr CastleRights& operator&=(CastleRights& lhs, CastleRights rhs) {
    lhs = lhs & rhs;
    return lhs;
}

namespace {
constexpr char pieces[] = {
    ' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ', ' ', 'P', 'N', 'B', 'R', 'Q', 'K', ' '};
};

constexpr char to_char(Piece p) {
    return pieces[p];
}

constexpr char to_char(PieceType pt) {
    return pieces[pt];
}

constexpr char to_char(Rank rank) {
    return static_cast<char>('1' + int(rank));
}

constexpr char to_char(File file) {
    return static_cast<char>('a' + int(file));
}

inline std::string to_string(Square sq) {
    return std::string{to_char(File(sq & 7))} + to_char(Rank(sq >> 3));
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

namespace masks {

constexpr uint64_t dark_squares   = 0xAA55AA55AA55AA55ull;
constexpr uint64_t light_squares  = 0x55AA55AA55AA55AAull;
constexpr uint64_t center_files   = 0x3C3C3C3C3C3C3C3Cull;
constexpr uint64_t center_squares = 0x0000001818000000ull;
constexpr uint64_t w_outposts     = 0x0000FFFFFF000000ull;
constexpr uint64_t b_outposts     = 0x000000FFFFFF0000ull;

}; // namespace masks

namespace castle {

constexpr Square rook_from[N_CASTLES][N_COLORS] = {
    {H8, H1}, // kingside
    {A8, A1}, // queenside
};

constexpr Square rook_to[N_CASTLES][N_COLORS] = {
    {F8, F1}, // kingside
    {D8, D1}, // queenside
};

constexpr uint64_t path[N_CASTLES][N_COLORS]{
    {0x6000000000000000ull, 0x0000000000000060ull}, // kingside
    {0x0E00000000000000ull, 0x000000000000000Eull}, // queenside
};

constexpr uint64_t kingpath[N_CASTLES][N_COLORS]{
    {0x7000000000000000ull, 0x0000000000000070ull}, // kingside
    {0x1C00000000000000ull, 0x000000000000001Cull}, // queenside
};

}; // namespace castle
