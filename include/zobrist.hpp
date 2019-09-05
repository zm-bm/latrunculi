#ifndef ANTONIUS_ZOBRIST_H
#define ANTONIUS_ZOBRIST_H

#include "types.hpp"

namespace Zobrist
{

    void init();
    
    extern U64 psq[NCOLORS][NPIECETYPES][NSQUARES];
    extern U64 stm;
    extern U64 ep[8];
    extern U64 castle[NCOLORS][2];

}

#endif