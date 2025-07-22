#pragma once

#include "board.hpp"
#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

constexpr int MAX_HISTORY = static_cast<int>(HISTORY_MOVE);

struct HistoryTable {
    I16 history[N_COLORS][N_SQUARES][N_SQUARES] = {0};

    void update(Color c, Square from, Square to, int depth) {
        int bonus   = depth * depth;
        int clamped = std::clamp(bonus, -MAX_HISTORY, MAX_HISTORY);
        int gravity = bonus - (history[c][from][to] * bonus / MAX_HISTORY);

        history[c][from][to] += gravity;
    }

    int get(Color c, Square from, Square to) const { return history[c][from][to]; }

    void age() {
        for (int c = 0; c < N_COLORS; ++c)
            for (int from = 0; from < N_SQUARES; ++from)
                for (int to = 0; to < N_SQUARES; ++to) history[c][from][to] >>= 1;
    }

    void clear() {
        for (int c = 0; c < N_COLORS; ++c)
            for (int from = 0; from < N_SQUARES; ++from)
                for (int to = 0; to < N_SQUARES; ++to) history[c][from][to] = 0;
    }
};

struct KillerMoves {
    Move killers[MAX_DEPTH][2] = {NullMove};

    void update(Move killer, int ply) {
        if (killers[ply][0] == killer) return;
        killers[ply][1] = killers[ply][0];
        killers[ply][0] = killer;
    }

    bool isKiller(Move move, int ply) const {
        return move == killers[ply][0] || move == killers[ply][1];
    }

    void clear() {
        for (int ply = 0; ply < MAX_DEPTH; ++ply) {
            killers[ply][0] = NullMove;
            killers[ply][1] = NullMove;
        }
    }
};
