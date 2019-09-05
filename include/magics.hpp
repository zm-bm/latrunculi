#ifndef ANTONIUS_MAGICS_H
#define ANTONIUS_MAGICS_H

#include "types.hpp"

namespace Magic
{

    extern const U64* rookAttacks[64];
    extern const U64 rookMagicNum[64];
    extern const U64 rookMagicMask[64];
    extern const int rookMagicShift[64];

    extern const U64* bishopAttacks[64];
    extern const U64 bishopMagicNum[64];
    extern const U64 bishopMagicMask[64];
    extern const int bishopMagicShift[64];

    void init();

    U64 init_occupied(int* squares, int numSquares, U64 line_occupied);
    U64 init_magic_bishop(int square, U64 occupied);
    U64 init_magic_rook(int square, U64 occupied);

    inline U64 getRookAttacks(Square sq, U64 occ)
    {
        auto occupancy = occ & Magic::rookMagicMask[sq];
        auto index = (occupancy * Magic::rookMagicNum[sq]) >> Magic::rookMagicShift[sq];
        return Magic::rookAttacks[sq][index];
    }

    inline U64 getBishopAttacks(Square sq, U64 occ)
    {
        auto occupancy = occ & Magic::bishopMagicMask[sq];
        auto index = (occupancy * Magic::bishopMagicNum[sq]) >> Magic::bishopMagicShift[sq];
        return Magic::bishopAttacks[sq][index];
    }

    inline U64 getQueenAttacks(Square sq, U64 occ)
    {
        return getBishopAttacks(sq, occ) | getRookAttacks(sq, occ);
    }

}

#endif