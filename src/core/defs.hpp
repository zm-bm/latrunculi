#pragma once

#include <cstdint>

#include "core/types.hpp"

#ifndef LATRUNCULI_VERSION
#define LATRUNCULI_VERSION "0.0.0"
#endif

constexpr int MAX_SEARCH_PLY = 128;

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

namespace masks {

constexpr uint64_t dark_squares   = 0xAA55AA55AA55AA55ull;
constexpr uint64_t light_squares  = 0x55AA55AA55AA55AAull;
constexpr uint64_t center_files   = 0x3C3C3C3C3C3C3C3Cull;
constexpr uint64_t center_squares = 0x0000001818000000ull;
constexpr uint64_t w_outposts     = 0x0000FFFFFF000000ull;
constexpr uint64_t b_outposts     = 0x000000FFFFFF0000ull;

}; // namespace masks
