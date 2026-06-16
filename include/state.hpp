#pragma once

#include "defs.hpp"
#include "move.hpp"

/**
 * Reversible ply-local board state.
 *
 * Check data is recomputed after load/make/null moves. The remaining fields
 * are copied from the previous ply as needed, then adjusted by Board::make().
 */
struct State {
    State() = default;
    State(const State& state, Move mv);

    // Derived check data.
    uint64_t checkers = 0;
    // blockers[c]: the sole occupied square, if any, between king c and an enemy slider.
    // Own blockers are pinned; enemy blockers can move to uncover check.
    uint64_t blockers[N_COLORS] = {0};
    // pinners[c]: sliders of color c pinning an enemy blocker to its king.
    // Farther snipers behind other snipers are included; direct checks are not.
    uint64_t pinners[N_COLORS] = {0};
    // Direct-check masks indexed by NO_PIECETYPE..QUEEN; KING maps to zero.
    uint64_t checks[N_PIECES - 1] = {0};

    // Incremental and move-local data. Order is kept compact.
    uint64_t     zkey         = 0;
    Move         move         = NULL_MOVE;
    PieceType    captured     = NO_PIECETYPE;
    CastleRights castle       = NO_CASTLE;
    Square       enpassant    = INVALID;
    uint8_t      halfmove_clk = 0;
};

inline State::State(const State& state, Move mv)
    : zkey(state.zkey),
      move(mv),
      castle(state.castle),
      halfmove_clk(state.halfmove_clk + 1) {}
