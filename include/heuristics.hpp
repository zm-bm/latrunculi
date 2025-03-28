#pragma once

#include "board.hpp"
#include "constants.hpp"
#include "move.hpp"

struct HistoryTable {
    int history[N_COLORS][N_SQUARES][N_SQUARES] = {0};

    inline void update(Color c, Square from, Square to, int ply) {
        int bonus             = std::clamp(1 << ply, -MAX_HISTORY, MAX_HISTORY);
        history[c][from][to] += bonus - (history[c][from][to] * bonus / MAX_HISTORY);
    }

    inline int get(Color c, Square from, Square to) const { return history[c][from][to]; }

    inline void age() {
        for (int c = 0; c < N_COLORS; ++c)
            for (int from = 0; from < N_SQUARES; ++from)
                for (int to = 0; to < N_SQUARES; ++to)
                    history[c][from][to] >>= 1;  // age history values
    }
};

struct KillerMoves {
    Move killers[MAX_DEPTH][2];

    inline void update(Move killer, int ply) {
        if (killers[ply][0] != killer) {
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = killer;
        }
    }

    inline bool isKiller(Move move, int ply) const {
        return move == killers[ply][0] || move == killers[ply][1];
    }
};

struct Heuristics {
    HistoryTable history;
    KillerMoves killers;

    inline void addBetaCutoff(Board& board, Move move, int ply) {
        Square to = move.to();
        if (board.pieceOn(to) == Piece::NONE) {
            killers.update(move, ply);
            history.update(board.sideToMove(), move.from(), to, ply);
        }
    }
    inline void age() { history.age(); }
};
