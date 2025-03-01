#pragma once

#include "move.hpp"
#include "types.hpp"
#include "constants.hpp"
#include "zobrist.hpp"

class Board;

struct State {
    State() = default;
    State(const State&) = default;

    State(const State& state, Move mv)
        : zkey(state.zkey)
        , move(mv)
        , castle(state.castle)
        , hmClock(state.hmClock + 1) {}

    // Check info bitboards
    U64 checkingPieces = 0;
    U64 pinnedPieces = 0;
    U64 discoveredCheckers = 0;
    U64 checkingSquares[N_PIECES-1] = {0, 0, 0, 0, 0, 0};

    // Hash keys
    U64 zkey = 0;

    // Board state variables
    Move move;
    PieceType captured = NO_PIECE_TYPE;
    CastleRights castle = ALL_CASTLE;
    Square enPassantSq = INVALID;
    U8 hmClock = 0;

    inline bool canCastle(Color c) const {
        // Check if the specified color can castle
        return (c ? castle & WHITE_CASTLE : castle & BLACK_CASTLE);
    }

    inline bool canCastleOO(Color c) const {
        // Check if the specified color can castle kingside (OO)
        return (c ? castle & WHITE_OO : castle & BLACK_OO);
    }

    inline bool canCastleOOO(Color c) const {
        // Check if the specified color can castle queenside (OOO)
        return (c ? castle & WHITE_OOO : castle & BLACK_OOO);
    }

    inline void disableCastle(Color c) {
        // Disable castling for the specified color
        if (canCastleOO(c)) zkey ^= Zobrist::castle[c][KINGSIDE];
        if (canCastleOOO(c)) zkey ^= Zobrist::castle[c][QUEENSIDE];

        // Update the castle rights based on the color
        castle &= c ? BLACK_CASTLE : WHITE_CASTLE;
    }

    inline void disableCastle(Color c, Square sq) {
        // Disable casting for the specified color and side
        if (sq == RookOriginOO[c] && canCastleOO(c)) {
            disableCastleOO(c);
        } else if (sq == RookOriginOOO[c] && canCastleOOO(c)) {
            disableCastleOOO(c);
        }
    }

    inline void disableCastleOO(Color c) {
        // Disable kingside castling for the specified color
        zkey ^= Zobrist::castle[c][KINGSIDE];
        castle &= c ? CastleRights(0x07) : CastleRights(0x0D);
    }

    inline void disableCastleOOO(Color c) {
        // Disable queenside castling for the specified color
        zkey ^= Zobrist::castle[c][QUEENSIDE];
        castle &= c ? CastleRights(0x0B) : CastleRights(0x0E);
    }
};
