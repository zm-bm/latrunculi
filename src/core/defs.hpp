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
