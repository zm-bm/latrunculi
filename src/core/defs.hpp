#pragma once

#include <cstdint>

#include "core/constants.hpp"
#include "core/types.hpp"

enum Value : int16_t {
    DRAW_VALUE    = 0,
    INF_VALUE     = INT16_MAX,
    MATE_VALUE    = INT16_MAX - 1,
    MATE_BOUND    = MATE_VALUE - engine::max_search_ply,
    TT_MATE_BOUND = MATE_VALUE - 2 * engine::max_search_ply,

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
