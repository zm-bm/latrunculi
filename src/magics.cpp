#include "types.hpp"
#include "bitboard.hpp"
#include "magics.hpp"

/**
 * Modified implementation of fancy magic bitboards for Latrunculi
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

namespace Magic {

    // Rook magics
    U64 rookAttacksDB[102400];

    const U64* rookAttacks[64] = {
        rookAttacksDB + 86016, rookAttacksDB + 73728,
        rookAttacksDB + 36864, rookAttacksDB + 43008,
        rookAttacksDB + 47104, rookAttacksDB + 51200,
        rookAttacksDB + 77824, rookAttacksDB + 94208,
        rookAttacksDB + 69632, rookAttacksDB + 32768,
        rookAttacksDB + 38912, rookAttacksDB + 10240,
        rookAttacksDB + 14336, rookAttacksDB + 53248,
        rookAttacksDB + 57344, rookAttacksDB + 81920,
        rookAttacksDB + 24576, rookAttacksDB + 33792,
        rookAttacksDB + 6144, rookAttacksDB + 11264,
        rookAttacksDB + 15360, rookAttacksDB + 18432,
        rookAttacksDB + 58368, rookAttacksDB + 61440,
        rookAttacksDB + 26624, rookAttacksDB + 4096,
        rookAttacksDB + 7168, rookAttacksDB + 0,
        rookAttacksDB + 2048, rookAttacksDB + 19456,
        rookAttacksDB + 22528, rookAttacksDB + 63488,
        rookAttacksDB + 28672, rookAttacksDB + 5120,
        rookAttacksDB + 8192, rookAttacksDB + 1024,
        rookAttacksDB + 3072, rookAttacksDB + 20480,
        rookAttacksDB + 23552, rookAttacksDB + 65536,
        rookAttacksDB + 30720, rookAttacksDB + 34816,
        rookAttacksDB + 9216, rookAttacksDB + 12288,
        rookAttacksDB + 16384, rookAttacksDB + 21504,
        rookAttacksDB + 59392, rookAttacksDB + 67584,
        rookAttacksDB + 71680, rookAttacksDB + 35840,
        rookAttacksDB + 39936, rookAttacksDB + 13312,
        rookAttacksDB + 17408, rookAttacksDB + 54272,
        rookAttacksDB + 60416, rookAttacksDB + 83968,
        rookAttacksDB + 90112, rookAttacksDB + 75776,
        rookAttacksDB + 40960, rookAttacksDB + 45056,
        rookAttacksDB + 49152, rookAttacksDB + 55296,
        rookAttacksDB + 79872, rookAttacksDB + 98304
    };
    
    const U64 rookMagicNum[64] = {
        0x0080001020400080ull, 0x0040001000200040ull, 0x0080081000200080ull,
        0x0080040800100080ull, 0x0080020400080080ull, 0x0080010200040080ull,
        0x0080008001000200ull, 0x0080002040800100ull, 0x0000800020400080ull,
        0x0000400020005000ull, 0x0000801000200080ull, 0x0000800800100080ull,
        0x0000800400080080ull, 0x0000800200040080ull, 0x0000800100020080ull,
        0x0000800040800100ull, 0x0000208000400080ull, 0x0000404000201000ull,
        0x0000808010002000ull, 0x0000808008001000ull, 0x0000808004000800ull,
        0x0000808002000400ull, 0x0000010100020004ull, 0x0000020000408104ull,
        0x0000208080004000ull, 0x0000200040005000ull, 0x0000100080200080ull,
        0x0000080080100080ull, 0x0000040080080080ull, 0x0000020080040080ull,
        0x0000010080800200ull, 0x0000800080004100ull, 0x0000204000800080ull,
        0x0000200040401000ull, 0x0000100080802000ull, 0x0000080080801000ull,
        0x0000040080800800ull, 0x0000020080800400ull, 0x0000020001010004ull,
        0x0000800040800100ull, 0x0000204000808000ull, 0x0000200040008080ull,
        0x0000100020008080ull, 0x0000080010008080ull, 0x0000040008008080ull,
        0x0000020004008080ull, 0x0000010002008080ull, 0x0000004081020004ull,
        0x0000204000800080ull, 0x0000200040008080ull, 0x0000100020008080ull,
        0x0000080010008080ull, 0x0000040008008080ull, 0x0000020004008080ull,
        0x0000800100020080ull, 0x0000800041000080ull, 0x00FFFCDDFCED714Aull,
        0x007FFCDDFCED714Aull, 0x003FFFCDFFD88096ull, 0x0000040810002101ull,
        0x0001000204080011ull, 0x0001000204000801ull, 0x0001000082000401ull,
        0x0001FFFAABFAD1A2ull
    };

    const U64 rookMagicMask[64] = {
        0x000101010101017Eull, 0x000202020202027Cull, 0x000404040404047Aull,
        0x0008080808080876ull, 0x001010101010106Eull, 0x002020202020205Eull,
        0x004040404040403Eull, 0x008080808080807Eull, 0x0001010101017E00ull,
        0x0002020202027C00ull, 0x0004040404047A00ull, 0x0008080808087600ull,
        0x0010101010106E00ull, 0x0020202020205E00ull, 0x0040404040403E00ull,
        0x0080808080807E00ull, 0x00010101017E0100ull, 0x00020202027C0200ull,
        0x00040404047A0400ull, 0x0008080808760800ull, 0x00101010106E1000ull,
        0x00202020205E2000ull, 0x00404040403E4000ull, 0x00808080807E8000ull,
        0x000101017E010100ull, 0x000202027C020200ull, 0x000404047A040400ull,
        0x0008080876080800ull, 0x001010106E101000ull, 0x002020205E202000ull,
        0x004040403E404000ull, 0x008080807E808000ull, 0x0001017E01010100ull,
        0x0002027C02020200ull, 0x0004047A04040400ull, 0x0008087608080800ull,
        0x0010106E10101000ull, 0x0020205E20202000ull, 0x0040403E40404000ull,
        0x0080807E80808000ull, 0x00017E0101010100ull, 0x00027C0202020200ull, 
        0x00047A0404040400ull, 0x0008760808080800ull, 0x00106E1010101000ull,
        0x00205E2020202000ull, 0x00403E4040404000ull, 0x00807E8080808000ull,
        0x007E010101010100ull, 0x007C020202020200ull, 0x007A040404040400ull,
        0x0076080808080800ull, 0x006E101010101000ull, 0x005E202020202000ull,
        0x003E404040404000ull, 0x007E808080808000ull, 0x7E01010101010100ull,
        0x7C02020202020200ull, 0x7A04040404040400ull, 0x7608080808080800ull,
        0x6E10101010101000ull, 0x5E20202020202000ull, 0x3E40404040404000ull,
        0x7E80808080808000ull
    };
    
    const int rookMagicShift[64] = {
        52, 53, 53, 53, 53, 53, 53, 52,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 54, 54, 54, 54, 53,
        53, 54, 54, 53, 53, 53, 53, 53
    };

    // Bishop magics
    U64 bishopAttacksDB[5248];

    const U64* bishopAttacks[64] = {
        bishopAttacksDB + 4992, bishopAttacksDB + 2624,
        bishopAttacksDB + 256, bishopAttacksDB + 896,
        bishopAttacksDB + 1280, bishopAttacksDB + 1664,
        bishopAttacksDB + 4800, bishopAttacksDB + 5120,
        bishopAttacksDB + 2560, bishopAttacksDB + 2656,
        bishopAttacksDB + 288, bishopAttacksDB + 928,
        bishopAttacksDB + 1312, bishopAttacksDB + 1696,
        bishopAttacksDB + 4832, bishopAttacksDB + 4928,
        bishopAttacksDB + 0, bishopAttacksDB + 128,
        bishopAttacksDB + 320, bishopAttacksDB + 960,
        bishopAttacksDB + 1344, bishopAttacksDB + 1728,
        bishopAttacksDB + 2304, bishopAttacksDB + 2432,
        bishopAttacksDB + 32, bishopAttacksDB + 160,
        bishopAttacksDB + 448, bishopAttacksDB + 2752,
        bishopAttacksDB + 3776, bishopAttacksDB + 1856,
        bishopAttacksDB + 2336, bishopAttacksDB + 2464,
        bishopAttacksDB + 64, bishopAttacksDB + 192,
        bishopAttacksDB + 576, bishopAttacksDB + 3264,
        bishopAttacksDB + 4288, bishopAttacksDB + 1984,
        bishopAttacksDB + 2368, bishopAttacksDB + 2496,
        bishopAttacksDB + 96, bishopAttacksDB + 224,
        bishopAttacksDB + 704, bishopAttacksDB + 1088,
        bishopAttacksDB + 1472, bishopAttacksDB + 2112,
        bishopAttacksDB + 2400, bishopAttacksDB + 2528,
        bishopAttacksDB + 2592, bishopAttacksDB + 2688,
        bishopAttacksDB + 832, bishopAttacksDB + 1216,
        bishopAttacksDB + 1600, bishopAttacksDB + 2240,
        bishopAttacksDB + 4864, bishopAttacksDB + 4960,
        bishopAttacksDB + 5056, bishopAttacksDB + 2720,
        bishopAttacksDB + 864, bishopAttacksDB + 1248,
        bishopAttacksDB + 1632, bishopAttacksDB + 2272,
        bishopAttacksDB + 4896, bishopAttacksDB + 5184
    };

    const U64 bishopMagicNum[64] = {
        0x0002020202020200ull, 0x0002020202020000ull, 0x0004010202000000ull,
        0x0004040080000000ull, 0x0001104000000000ull, 0x0000821040000000ull,
        0x0000410410400000ull, 0x0000104104104000ull, 0x0000040404040400ull,
        0x0000020202020200ull, 0x0000040102020000ull, 0x0000040400800000ull,
        0x0000011040000000ull, 0x0000008210400000ull, 0x0000004104104000ull,
        0x0000002082082000ull, 0x0004000808080800ull, 0x0002000404040400ull,
        0x0001000202020200ull, 0x0000800802004000ull, 0x0000800400A00000ull,
        0x0000200100884000ull, 0x0000400082082000ull, 0x0000200041041000ull,
        0x0002080010101000ull, 0x0001040008080800ull, 0x0000208004010400ull,
        0x0000404004010200ull, 0x0000840000802000ull, 0x0000404002011000ull,
        0x0000808001041000ull, 0x0000404000820800ull, 0x0001041000202000ull,
        0x0000820800101000ull, 0x0000104400080800ull, 0x0000020080080080ull,
        0x0000404040040100ull, 0x0000808100020100ull, 0x0001010100020800ull,
        0x0000808080010400ull, 0x0000820820004000ull, 0x0000410410002000ull,
        0x0000082088001000ull, 0x0000002011000800ull, 0x0000080100400400ull,
        0x0001010101000200ull, 0x0002020202000400ull, 0x0001010101000200ull,
        0x0000410410400000ull, 0x0000208208200000ull, 0x0000002084100000ull,
        0x0000000020880000ull, 0x0000001002020000ull, 0x0000040408020000ull,
        0x0004040404040000ull, 0x0002020202020000ull, 0x0000104104104000ull,
        0x0000002082082000ull, 0x0000000020841000ull, 0x0000000000208800ull,
        0x0000000010020200ull, 0x0000000404080200ull, 0x0000040404040400ull,
        0x0002020202020200ull
    };

    const U64 bishopMagicMask[64] = {
        0x0040201008040200ull, 0x0000402010080400ull, 0x0000004020100A00ull,
        0x0000000040221400ull, 0x0000000002442800ull, 0x0000000204085000ull,
        0x0000020408102000ull, 0x0002040810204000ull, 0x0020100804020000ull,
        0x0040201008040000ull, 0x00004020100A0000ull, 0x0000004022140000ull,
        0x0000000244280000ull, 0x0000020408500000ull, 0x0002040810200000ull,
        0x0004081020400000ull, 0x0010080402000200ull, 0x0020100804000400ull,
        0x004020100A000A00ull, 0x0000402214001400ull, 0x0000024428002800ull,
        0x0002040850005000ull, 0x0004081020002000ull, 0x0008102040004000ull,
        0x0008040200020400ull, 0x0010080400040800ull, 0x0020100A000A1000ull,
        0x0040221400142200ull, 0x0002442800284400ull, 0x0004085000500800ull,
        0x0008102000201000ull, 0x0010204000402000ull, 0x0004020002040800ull,
        0x0008040004081000ull, 0x00100A000A102000ull, 0x0022140014224000ull,
        0x0044280028440200ull, 0x0008500050080400ull, 0x0010200020100800ull,
        0x0020400040201000ull, 0x0002000204081000ull, 0x0004000408102000ull,
        0x000A000A10204000ull, 0x0014001422400000ull, 0x0028002844020000ull,
        0x0050005008040200ull, 0x0020002010080400ull, 0x0040004020100800ull,
        0x0000020408102000ull, 0x0000040810204000ull, 0x00000A1020400000ull,
        0x0000142240000000ull, 0x0000284402000000ull, 0x0000500804020000ull,
        0x0000201008040200ull, 0x0000402010080400ull, 0x0002040810204000ull,
        0x0004081020400000ull, 0x000A102040000000ull, 0x0014224000000000ull,
        0x0028440200000000ull, 0x0050080402000000ull, 0x0020100804020000ull,
        0x0040201008040200ull
	};

    const int bishopMagicShift[64] = {
        58, 59, 59, 59, 59, 59, 59, 58,
        59, 59, 59, 59, 59, 59, 59, 59,
        59, 59, 57, 57, 57, 57, 59, 59,
        59, 59, 57, 55, 55, 57, 59, 59,
        59, 59, 57, 55, 55, 57, 59, 59,
        59, 59, 57, 57, 57, 57, 59, 59,
        59, 59, 59, 59, 59, 59, 59, 59,
        58, 59, 59, 59, 59, 59, 59, 58
    };

    void init()
    {
        U64* rookAttacksInit[64] = {
            rookAttacksDB + 86016, rookAttacksDB + 73728,
            rookAttacksDB + 36864, rookAttacksDB + 43008,
            rookAttacksDB + 47104, rookAttacksDB + 51200,
            rookAttacksDB + 77824, rookAttacksDB + 94208,
            rookAttacksDB + 69632, rookAttacksDB + 32768,
            rookAttacksDB + 38912, rookAttacksDB + 10240,
            rookAttacksDB + 14336, rookAttacksDB + 53248,
            rookAttacksDB + 57344, rookAttacksDB + 81920,
            rookAttacksDB + 24576, rookAttacksDB + 33792,
            rookAttacksDB + 6144, rookAttacksDB + 11264,
            rookAttacksDB + 15360, rookAttacksDB + 18432,
            rookAttacksDB + 58368, rookAttacksDB + 61440,
            rookAttacksDB + 26624, rookAttacksDB + 4096,
            rookAttacksDB + 7168, rookAttacksDB + 0,
            rookAttacksDB + 2048, rookAttacksDB + 19456,
            rookAttacksDB + 22528, rookAttacksDB + 63488,
            rookAttacksDB + 28672, rookAttacksDB + 5120,
            rookAttacksDB + 8192, rookAttacksDB + 1024,
            rookAttacksDB + 3072, rookAttacksDB + 20480,
            rookAttacksDB + 23552, rookAttacksDB + 65536,
            rookAttacksDB + 30720, rookAttacksDB + 34816,
            rookAttacksDB + 9216, rookAttacksDB + 12288,
            rookAttacksDB + 16384, rookAttacksDB + 21504,
            rookAttacksDB + 59392, rookAttacksDB + 67584,
            rookAttacksDB + 71680, rookAttacksDB + 35840,
            rookAttacksDB + 39936, rookAttacksDB + 13312,
            rookAttacksDB + 17408, rookAttacksDB + 54272,
            rookAttacksDB + 60416, rookAttacksDB + 83968,
            rookAttacksDB + 90112, rookAttacksDB + 75776,
            rookAttacksDB + 40960, rookAttacksDB + 45056,
            rookAttacksDB + 49152, rookAttacksDB + 55296,
            rookAttacksDB + 79872, rookAttacksDB + 98304
        };
        U64* bishopAttacksInit[64] = {
            bishopAttacksDB + 4992, bishopAttacksDB + 2624,
            bishopAttacksDB + 256, bishopAttacksDB + 896,
            bishopAttacksDB + 1280, bishopAttacksDB + 1664,
            bishopAttacksDB + 4800, bishopAttacksDB + 5120,
            bishopAttacksDB + 2560, bishopAttacksDB + 2656,
            bishopAttacksDB + 288, bishopAttacksDB + 928,
            bishopAttacksDB + 1312, bishopAttacksDB + 1696,
            bishopAttacksDB + 4832, bishopAttacksDB + 4928,
            bishopAttacksDB + 0, bishopAttacksDB + 128,
            bishopAttacksDB + 320, bishopAttacksDB + 960,
            bishopAttacksDB + 1344, bishopAttacksDB + 1728,
            bishopAttacksDB + 2304, bishopAttacksDB + 2432,
            bishopAttacksDB + 32, bishopAttacksDB + 160,
            bishopAttacksDB + 448, bishopAttacksDB + 2752,
            bishopAttacksDB + 3776, bishopAttacksDB + 1856,
            bishopAttacksDB + 2336, bishopAttacksDB + 2464,
            bishopAttacksDB + 64, bishopAttacksDB + 192,
            bishopAttacksDB + 576, bishopAttacksDB + 3264,
            bishopAttacksDB + 4288, bishopAttacksDB + 1984,
            bishopAttacksDB + 2368, bishopAttacksDB + 2496,
            bishopAttacksDB + 96, bishopAttacksDB + 224,
            bishopAttacksDB + 704, bishopAttacksDB + 1088,
            bishopAttacksDB + 1472, bishopAttacksDB + 2112,
            bishopAttacksDB + 2400, bishopAttacksDB + 2528,
            bishopAttacksDB + 2592, bishopAttacksDB + 2688,
            bishopAttacksDB + 832, bishopAttacksDB + 1216,
            bishopAttacksDB + 1600, bishopAttacksDB + 2240,
            bishopAttacksDB + 4864, bishopAttacksDB + 4960,
            bishopAttacksDB + 5056, bishopAttacksDB + 2720,
            bishopAttacksDB + 864, bishopAttacksDB + 1248,
            bishopAttacksDB + 1632, bishopAttacksDB + 2272,
            bishopAttacksDB + 4896, bishopAttacksDB + 5184
        };

        // Pre-calculate bishop attacks with magic bitboards
        for (int i = 0; i < 64; i++) {
            int squares[64];
            int numsquares = 0;
            U64 temp = bishopMagicMask[i];

            while (temp) {
                U64 lsb = temp & -temp;
                squares[numsquares++] = BB(lsb).lsb();
                temp ^= lsb;
            }
            for (temp = 0; temp < (U64) 1 << numsquares; temp++) {
                U64 moves;
                U64 tempoccupied = init_occupied(squares, numsquares, temp);
                moves = init_magic_bishop(i, tempoccupied);
                *(bishopAttacksInit[i] + (tempoccupied * bishopMagicNum[i] >> bishopMagicShift[i])) = moves;
            }
        }

        // Pre-calculate rook attacks with magic bitboards
        for (int i = 0; i < 64; i++) {
            int squares[64];
            int numsquares = 0;
            U64 temp = rookMagicMask[i];

            while (temp) {
                U64 lsb = temp & -temp;
                squares[numsquares++] = BB(lsb).lsb();
                temp ^= lsb;
            }
            for (temp = 0; temp < (U64) 1 << numsquares; temp++) {
                U64 tempoccupied = init_occupied(squares, numsquares, temp);
                U64 moves = init_magic_rook(i, tempoccupied);
                *(rookAttacksInit[i] + (tempoccupied * rookMagicNum[i] >> rookMagicShift[i])) = moves;
            }
        }
    }

    // Translates line occupancy into an occupied-square bitboard
    U64 init_occupied(int* squares, int numSquares, U64 line_occupied) {
        int i;
        U64 ret = 0;

        for (i = 0; i < numSquares; i++)
            if (line_occupied & (U64) 1 << i)
            ret |= (U64) 1 << squares[i];
        return ret;
    }

    U64 init_magic_bishop(int square, U64 occupied) {
        U64 ret = 0;
        U64 bit;
        U64 bit2;
        U64 rowbits = (U64) 0xFF << (8 * (square / 8));

        bit = (U64) 1 << square;
        bit2 = bit;
        do {
            bit <<= 8 - 1;
            bit2 >>= 1;
            if (bit2 & rowbits)
                ret |= bit;
            else
                break;
        } while (bit && !(bit & occupied));

        bit = (U64) 1 << square;
        bit2 = bit;
        do {
            bit <<= 8 + 1;
            bit2 <<= 1;
            if (bit2 & rowbits)
                ret |= bit;
            else
                break;
        } while (bit && !(bit & occupied));

        bit = (U64) 1 << square;
        bit2 = bit;
        do {
            bit >>= 8 - 1;
            bit2 <<= 1;
            if (bit2 & rowbits)
                ret |= bit;
            else
                break;
        } while (bit && !(bit & occupied));

        bit = (U64) 1 << square;
        bit2 = bit;
        do {
            bit >>= 8 + 1;
            bit2 >>= 1;
            if (bit2 & rowbits)
                ret |= bit;
            else
                break;
        } while (bit && !(bit & occupied));

        return ret;
    }

    U64 init_magic_rook(int square, U64 occupied) {
        U64 ret = 0;
        U64 bit;
        U64 rowbits = (U64) 0xFF << 8 * (square / 8);

        bit = (U64) 1 << square;
        do {
            bit <<= 8;
            ret |= bit;
        } while (bit && !(bit & occupied));

        bit = (U64) 1 << square;
        do {
            bit >>= 8;
            ret |= bit;
        } while (bit && !(bit & occupied));

        bit = (U64) 1 << square;
        do {
            bit <<= 1;
            if (bit & rowbits)
                ret |= bit;
            else
                break;
        } while (!(bit & occupied));

        bit = (U64) 1 << square;
        do {
            bit >>= 1;
            if (bit & rowbits)
                ret |= bit;
            else
                break;
        } while (!(bit & occupied));

        return ret;
    }

}