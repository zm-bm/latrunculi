#pragma once

#include "core/move_geometry.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

namespace zob {

void init();

extern PositionKey piece[N_COLORS][N_PIECETYPES][N_SQUARES];
extern PositionKey turn;
extern PositionKey ep[8];
extern PositionKey castle[N_CASTLES][N_COLORS];

inline PositionKey hash_piece(const Color c, const PieceType pt, const Square sq) {
    return piece[c][pt][sq];
}

inline PositionKey hash_ep(const Square sq) {
    return ep[square::file_of(sq)];
}

} // namespace zob
