#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "core/types.hpp"

#ifndef LATRUNCULI_VERSION
#define LATRUNCULI_VERSION "0.0.0"
#endif

#ifndef LATRUNCULI_SEARCH_STATS
#define LATRUNCULI_SEARCH_STATS 0
#endif

using Clock        = std::chrono::high_resolution_clock;
using TimePoint    = std::chrono::high_resolution_clock::time_point;
using Milliseconds = std::chrono::milliseconds;

constexpr int  MAX_SEARCH_DEPTH = 64;
constexpr int  MAX_SEARCH_PLY   = 128;
constexpr int  MAX_KEY_HISTORY  = 256;
constexpr int  MAX_MOVES        = 256;
constexpr int  DEFAULT_THREADS  = 1;
constexpr int  DEFAULT_HASH     = 4;
constexpr bool SEARCH_STATS     = LATRUNCULI_SEARCH_STATS;

enum Value : int16_t {
    DRAW_VALUE    = 0,
    INF_VALUE     = INT16_MAX,
    MATE_VALUE    = INT16_MAX - 1,
    MATE_BOUND    = MATE_VALUE - MAX_SEARCH_PLY,
    TT_MATE_BOUND = MATE_VALUE - 2 * MAX_SEARCH_PLY,

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

enum NodeType { PV, NON_PV };

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
