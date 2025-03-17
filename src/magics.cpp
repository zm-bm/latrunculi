#include "magics.hpp"

#include "bb.hpp"
#include "types.hpp"

/**
 * Modified implementation of magic bitboards for Latrunculi
 * All credit goes to Pradyumna Kannan
 *
 * Source file for magic move bitboard generation.
 *
 * The magic keys are not optimal for all squares but they are very close
 * to optimal.
 *
 * Copyright (C) 2007 Pradyumna Kannan.
 *
 * This code is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this code. Permission is granted to anyone to use this
 * code for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this code must not be misrepresented; you must not
 * claim that you wrote the original code. If you use this code in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original code.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

namespace Magics {
U64 rookAttacksDB[102400];
U64 bishopAttacksDB[5248];

void init() {
    U64* rookAttacksInit[64] = {
        rookAttacksDB + 86016, rookAttacksDB + 73728, rookAttacksDB + 36864, rookAttacksDB + 43008,
        rookAttacksDB + 47104, rookAttacksDB + 51200, rookAttacksDB + 77824, rookAttacksDB + 94208,
        rookAttacksDB + 69632, rookAttacksDB + 32768, rookAttacksDB + 38912, rookAttacksDB + 10240,
        rookAttacksDB + 14336, rookAttacksDB + 53248, rookAttacksDB + 57344, rookAttacksDB + 81920,
        rookAttacksDB + 24576, rookAttacksDB + 33792, rookAttacksDB + 6144,  rookAttacksDB + 11264,
        rookAttacksDB + 15360, rookAttacksDB + 18432, rookAttacksDB + 58368, rookAttacksDB + 61440,
        rookAttacksDB + 26624, rookAttacksDB + 4096,  rookAttacksDB + 7168,  rookAttacksDB + 0,
        rookAttacksDB + 2048,  rookAttacksDB + 19456, rookAttacksDB + 22528, rookAttacksDB + 63488,
        rookAttacksDB + 28672, rookAttacksDB + 5120,  rookAttacksDB + 8192,  rookAttacksDB + 1024,
        rookAttacksDB + 3072,  rookAttacksDB + 20480, rookAttacksDB + 23552, rookAttacksDB + 65536,
        rookAttacksDB + 30720, rookAttacksDB + 34816, rookAttacksDB + 9216,  rookAttacksDB + 12288,
        rookAttacksDB + 16384, rookAttacksDB + 21504, rookAttacksDB + 59392, rookAttacksDB + 67584,
        rookAttacksDB + 71680, rookAttacksDB + 35840, rookAttacksDB + 39936, rookAttacksDB + 13312,
        rookAttacksDB + 17408, rookAttacksDB + 54272, rookAttacksDB + 60416, rookAttacksDB + 83968,
        rookAttacksDB + 90112, rookAttacksDB + 75776, rookAttacksDB + 40960, rookAttacksDB + 45056,
        rookAttacksDB + 49152, rookAttacksDB + 55296, rookAttacksDB + 79872, rookAttacksDB + 98304};
    U64* bishopAttacksInit[64] = {
        bishopAttacksDB + 4992, bishopAttacksDB + 2624, bishopAttacksDB + 256,
        bishopAttacksDB + 896,  bishopAttacksDB + 1280, bishopAttacksDB + 1664,
        bishopAttacksDB + 4800, bishopAttacksDB + 5120, bishopAttacksDB + 2560,
        bishopAttacksDB + 2656, bishopAttacksDB + 288,  bishopAttacksDB + 928,
        bishopAttacksDB + 1312, bishopAttacksDB + 1696, bishopAttacksDB + 4832,
        bishopAttacksDB + 4928, bishopAttacksDB + 0,    bishopAttacksDB + 128,
        bishopAttacksDB + 320,  bishopAttacksDB + 960,  bishopAttacksDB + 1344,
        bishopAttacksDB + 1728, bishopAttacksDB + 2304, bishopAttacksDB + 2432,
        bishopAttacksDB + 32,   bishopAttacksDB + 160,  bishopAttacksDB + 448,
        bishopAttacksDB + 2752, bishopAttacksDB + 3776, bishopAttacksDB + 1856,
        bishopAttacksDB + 2336, bishopAttacksDB + 2464, bishopAttacksDB + 64,
        bishopAttacksDB + 192,  bishopAttacksDB + 576,  bishopAttacksDB + 3264,
        bishopAttacksDB + 4288, bishopAttacksDB + 1984, bishopAttacksDB + 2368,
        bishopAttacksDB + 2496, bishopAttacksDB + 96,   bishopAttacksDB + 224,
        bishopAttacksDB + 704,  bishopAttacksDB + 1088, bishopAttacksDB + 1472,
        bishopAttacksDB + 2112, bishopAttacksDB + 2400, bishopAttacksDB + 2528,
        bishopAttacksDB + 2592, bishopAttacksDB + 2688, bishopAttacksDB + 832,
        bishopAttacksDB + 1216, bishopAttacksDB + 1600, bishopAttacksDB + 2240,
        bishopAttacksDB + 4864, bishopAttacksDB + 4960, bishopAttacksDB + 5056,
        bishopAttacksDB + 2720, bishopAttacksDB + 864,  bishopAttacksDB + 1248,
        bishopAttacksDB + 1632, bishopAttacksDB + 2272, bishopAttacksDB + 4896,
        bishopAttacksDB + 5184};

    // Pre-calculate bishop attacks with magic bitboards
    for (int i = 0; i < 64; i++) {
        int squares[64];
        int numsquares = 0;
        U64 temp       = bishopMagicMask[i];

        while (temp) {
            U64 bitboard           = temp & -temp;
            squares[numsquares++]  = BB::lsb(bitboard);
            temp                  ^= bitboard;
        }
        for (temp = 0; temp < (U64)1 << numsquares; temp++) {
            U64 moves;
            U64 tempoccupied = initOccupied(squares, numsquares, temp);
            moves            = initMagicBishop(i, tempoccupied);
            *(bishopAttacksInit[i] + (tempoccupied * bishopMagicNum[i] >> bishopMagicShift[i])) =
                moves;
        }
    }

    // Pre-calculate rook attacks with magic bitboards
    for (int i = 0; i < 64; i++) {
        int squares[64];
        int numsquares = 0;
        U64 temp       = rookMagicMask[i];

        while (temp) {
            U64 bitboard           = temp & -temp;
            squares[numsquares++]  = BB::lsb(bitboard);
            temp                  ^= bitboard;
        }
        for (temp = 0; temp < (U64)1 << numsquares; temp++) {
            U64 tempoccupied = initOccupied(squares, numsquares, temp);
            U64 moves        = initMagicRook(i, tempoccupied);
            *(rookAttacksInit[i] + (tempoccupied * rookMagicNum[i] >> rookMagicShift[i])) = moves;
        }
    }
}

// Translates line occupancy into an occupied-square bitboard
U64 initOccupied(int* squares, int numSquares, U64 line_occupied) {
    int i;
    U64 ret = 0;

    for (i = 0; i < numSquares; i++) {
        if (line_occupied & (U64)1 << i) {
            ret |= (U64)1 << squares[i];
        }
    }
    return ret;
}

U64 initMagicBishop(int square, U64 occupied) {
    U64 attacks  = 0ULL;
    U64 rankMask = BB::rank(rankOf(Square(square)));

    auto generateMoves = [&](int shift, bool moveLeft, bool fileLeft) {
        U64 bishopPos = (1ULL << square);
        U64 fileMask  = bishopPos;

        do {
            bishopPos = moveLeft ? (bishopPos << shift) : (bishopPos >> shift);
            fileMask  = fileLeft ? (fileMask << 1) : (fileMask >> 1);

            if (fileMask & rankMask)
                attacks |= bishopPos;
            else
                break;
        } while (bishopPos && !(bishopPos & occupied));
    };

    generateMoves(7, true, false);   // NW
    generateMoves(9, true, true);    // NE
    generateMoves(7, false, true);   // SW
    generateMoves(9, false, false);  // SE

    return attacks;
}

U64 initMagicRook(int square, U64 occupied) {
    U64 attacks  = 0ULL;
    U64 rankMask = BB::rank(rankOf(Square(square)));

    auto generateMoves = [&](int shift, bool checkBoundary, bool shiftLeft) {
        U64 rookPos = (1ULL << square);
        do {
            rookPos = shiftLeft ? (rookPos << shift) : (rookPos >> shift);
            if (!rookPos) break;
            if (checkBoundary && !(rookPos & rankMask)) break;
            attacks |= rookPos;
        } while (!(rookPos & occupied));
    };

    generateMoves(8, false, true);   // North
    generateMoves(8, false, false);  // South
    generateMoves(1, true, true);    // West
    generateMoves(1, true, false);   // East

    return attacks;
}

}  // namespace Magics