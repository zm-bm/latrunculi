#pragma once

#include "defs.hpp"
#include "move.hpp"

/**
 * Reversible ply-local position state owned by the caller.
 *
 * Check data is recomputed after load/make/null moves. The remaining fields
 * store the undo data needed to restore the prior ply.
 */
struct PositionState {
    PositionState() = default;
    PositionState(PositionState& prior_state, Move move);
    Move move() const;

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

    // Incremental key and move-local undo data. Order is kept compact.
    uint64_t     zkey         = 0;
    uint16_t     move_bits    = 0;
    PieceType    captured     = NO_PIECETYPE;
    CastleRights castle       = NO_CASTLE;
    Square       enpassant    = INVALID;
    uint8_t      halfmove_clk = 0;
};

inline PositionState::PositionState(PositionState& prior_state, Move move)
    : zkey(prior_state.zkey),
      move_bits(move.bits),
      castle(prior_state.castle),
      halfmove_clk(prior_state.halfmove_clk + 1) {}

inline Move PositionState::move() const {
    Move move;
    move.bits = move_bits;
    return move;
}
