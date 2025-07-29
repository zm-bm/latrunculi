#pragma once

#include "defs.hpp"
#include "move.hpp"

struct KillerMoves {
    Move killers[MAX_DEPTH][2] = {NULL_MOVE};

    void update(Move killer, int ply);
    bool is_killer(Move move, int ply) const;
    void clear();
};

inline void KillerMoves::update(Move killer, int ply) {
    if (killers[ply][0] == killer)
        return;
    killers[ply][1] = killers[ply][0];
    killers[ply][0] = killer;
}

inline bool KillerMoves::is_killer(Move move, int ply) const {
    return move == killers[ply][0] || move == killers[ply][1];
}

inline void KillerMoves::clear() {
    for (int ply = 0; ply < MAX_DEPTH; ++ply) {
        killers[ply][0] = NULL_MOVE;
        killers[ply][1] = NULL_MOVE;
    }
}
