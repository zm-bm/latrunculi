#pragma once

#include "defs.hpp"
#include "move.hpp"

struct State {
    State() = default;
    State(const State& state, Move mv);

    uint64_t checkers             = 0;
    uint64_t blockers[N_COLORS]   = {0};
    uint64_t pinners[N_COLORS]    = {0};
    uint64_t checks[N_PIECES - 1] = {0};

    uint64_t     zkey         = 0;
    Move         move         = NULL_MOVE;
    PieceType    captured     = NO_PIECETYPE;
    CastleRights castle       = ALL_CASTLE;
    Square       enpassant    = INVALID;
    uint8_t      halfmove_clk = 0;
};

inline State::State(const State& state, Move mv)
    : zkey(state.zkey),
      move(mv),
      castle(state.castle),
      halfmove_clk(state.halfmove_clk + 1) {}
