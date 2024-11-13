#ifndef LATRUNCULI_ZOBRIST_H
#define LATRUNCULI_ZOBRIST_H

#include "types.hpp"

namespace Zobrist {

void init();

extern U64 psq[NCOLORS][N_PIECE_TYPES][NSQUARES];
extern U64 stm;
extern U64 ep[8];
extern U64 castle[NCOLORS][2];

}  // namespace Zobrist

#endif