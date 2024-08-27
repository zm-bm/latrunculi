#ifndef LATRUNCULI_STATE_H
#define LATRUNCULI_STATE_H

#include "types.hpp"
#include "move.hpp"

class Board;

struct State {

    State() = default;
    State(const State&) = default;

    State(const State& state, Move mv)
        : zkey(state.zkey)
        , pawnzkey(state.pawnzkey)
        , move(mv)
        , castle(state.castle)
        , hmClock(state.hmClock + 1)
    { }

    // Check info bitboards
    BBz checkingPieces = 0;
    BBz pinnedPieces = 0;
    BBz discoveredCheckers = 0;
    BBz checkingSquares[6] = { 0, 0, 0, 0, 0, 0 };

    // Hash keys
    U64 zkey = 0;
	U64 pawnzkey = 0;

    // Board state variables
    Move move;
    PieceType captured = NONE;
    CastleRights castle = ALL_CASTLE;
    Square enPassantSq = INVALID;
    U8 hmClock = 0;
};

#endif