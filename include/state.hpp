#pragma once

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"
#include "zobrist.hpp"

struct State {
    State()             = default;
    State(const State&) = default;

    State(const State& state, Move mv)
        : zkey(state.zkey), move(mv), castle(state.castle), hmClock(state.hmClock + 1) {}

    // Check info bitboards
    U64 checkers             = 0;
    U64 blockers[N_COLORS]   = {0};
    U64 pinners[N_COLORS]    = {0};
    U64 checks[N_PIECES - 1] = {0};

    // Hash keys
    U64 zkey = 0;

    // Board state variables
    Move move;
    PieceType captured  = PieceType::None;
    CastleRights castle = ALL_CASTLE;
    Square enPassantSq  = INVALID;
    U8 hmClock          = 0;
};
