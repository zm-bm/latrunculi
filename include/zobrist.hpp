#pragma once

#include "defs.hpp"
#include "util.hpp"

namespace zob {

void init();

extern uint64_t piece[N_COLORS][N_PIECES][N_SQUARES];
extern uint64_t turn;
extern uint64_t ep[8];
extern uint64_t castle[2][N_COLORS];

inline uint64_t hash_piece(const Color c, const PieceType pt, const Square sq) {
    return piece[c][pt][sq];
}

inline uint64_t hash_ep(const Square sq) {
    return ep[file_of(sq)];
}

} // namespace zob
