#ifndef LATRUNCULI_ZOBRIST_H
#define LATRUNCULI_ZOBRIST_H

#include "types.hpp"

namespace Zobrist {

void init();

extern U64 psq[N_COLORS][N_PIECES][N_SQUARES];
extern U64 stm;
extern U64 ep[8];
extern U64 castle[N_COLORS][2];

}  // namespace Zobrist

#endif