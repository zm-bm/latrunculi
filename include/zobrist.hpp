#pragma once

#include "constants.hpp"
#include "types.hpp"

namespace Zobrist {

void init();

extern U64 psq[N_COLORS][N_PIECES][N_SQUARES];
extern U64 stm;
extern U64 ep[8];
extern U64 castle[N_COLORS][N_CASTLES];

inline U64 hashPiece(const Color c, const PieceType p, const Square sq) {
    return psq[c][idx(p)][sq];
}

inline U64 hashEp(const Square sq) { return ep[idx(fileOf(sq))]; }

inline U64 hashCastle(const Color c, const Castle side) { return castle[c][idx(side)]; }

}  // namespace Zobrist
