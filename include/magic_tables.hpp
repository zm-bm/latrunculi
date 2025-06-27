/**
 * Header file for magic move bitboard generation.
 * This file contains magic bitboard tables and constants from Pradyumna Kannan.
 * Portions may be modified for integration with Latrunculi.
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

#pragma once

#include "types.hpp"

namespace MagicTables {

extern U64 rookAttacksTable[102400];
extern U64 bishopAttacksTable[5248];

constexpr U64* rookAttacks[64] = {
    rookAttacksTable + 86016, rookAttacksTable + 73728, rookAttacksTable + 36864,
    rookAttacksTable + 43008, rookAttacksTable + 47104, rookAttacksTable + 51200,
    rookAttacksTable + 77824, rookAttacksTable + 94208, rookAttacksTable + 69632,
    rookAttacksTable + 32768, rookAttacksTable + 38912, rookAttacksTable + 10240,
    rookAttacksTable + 14336, rookAttacksTable + 53248, rookAttacksTable + 57344,
    rookAttacksTable + 81920, rookAttacksTable + 24576, rookAttacksTable + 33792,
    rookAttacksTable + 6144,  rookAttacksTable + 11264, rookAttacksTable + 15360,
    rookAttacksTable + 18432, rookAttacksTable + 58368, rookAttacksTable + 61440,
    rookAttacksTable + 26624, rookAttacksTable + 4096,  rookAttacksTable + 7168,
    rookAttacksTable + 0,     rookAttacksTable + 2048,  rookAttacksTable + 19456,
    rookAttacksTable + 22528, rookAttacksTable + 63488, rookAttacksTable + 28672,
    rookAttacksTable + 5120,  rookAttacksTable + 8192,  rookAttacksTable + 1024,
    rookAttacksTable + 3072,  rookAttacksTable + 20480, rookAttacksTable + 23552,
    rookAttacksTable + 65536, rookAttacksTable + 30720, rookAttacksTable + 34816,
    rookAttacksTable + 9216,  rookAttacksTable + 12288, rookAttacksTable + 16384,
    rookAttacksTable + 21504, rookAttacksTable + 59392, rookAttacksTable + 67584,
    rookAttacksTable + 71680, rookAttacksTable + 35840, rookAttacksTable + 39936,
    rookAttacksTable + 13312, rookAttacksTable + 17408, rookAttacksTable + 54272,
    rookAttacksTable + 60416, rookAttacksTable + 83968, rookAttacksTable + 90112,
    rookAttacksTable + 75776, rookAttacksTable + 40960, rookAttacksTable + 45056,
    rookAttacksTable + 49152, rookAttacksTable + 55296, rookAttacksTable + 79872,
    rookAttacksTable + 98304};

constexpr U64* bishopAttacks[64] = {
    bishopAttacksTable + 4992, bishopAttacksTable + 2624, bishopAttacksTable + 256,
    bishopAttacksTable + 896,  bishopAttacksTable + 1280, bishopAttacksTable + 1664,
    bishopAttacksTable + 4800, bishopAttacksTable + 5120, bishopAttacksTable + 2560,
    bishopAttacksTable + 2656, bishopAttacksTable + 288,  bishopAttacksTable + 928,
    bishopAttacksTable + 1312, bishopAttacksTable + 1696, bishopAttacksTable + 4832,
    bishopAttacksTable + 4928, bishopAttacksTable + 0,    bishopAttacksTable + 128,
    bishopAttacksTable + 320,  bishopAttacksTable + 960,  bishopAttacksTable + 1344,
    bishopAttacksTable + 1728, bishopAttacksTable + 2304, bishopAttacksTable + 2432,
    bishopAttacksTable + 32,   bishopAttacksTable + 160,  bishopAttacksTable + 448,
    bishopAttacksTable + 2752, bishopAttacksTable + 3776, bishopAttacksTable + 1856,
    bishopAttacksTable + 2336, bishopAttacksTable + 2464, bishopAttacksTable + 64,
    bishopAttacksTable + 192,  bishopAttacksTable + 576,  bishopAttacksTable + 3264,
    bishopAttacksTable + 4288, bishopAttacksTable + 1984, bishopAttacksTable + 2368,
    bishopAttacksTable + 2496, bishopAttacksTable + 96,   bishopAttacksTable + 224,
    bishopAttacksTable + 704,  bishopAttacksTable + 1088, bishopAttacksTable + 1472,
    bishopAttacksTable + 2112, bishopAttacksTable + 2400, bishopAttacksTable + 2528,
    bishopAttacksTable + 2592, bishopAttacksTable + 2688, bishopAttacksTable + 832,
    bishopAttacksTable + 1216, bishopAttacksTable + 1600, bishopAttacksTable + 2240,
    bishopAttacksTable + 4864, bishopAttacksTable + 4960, bishopAttacksTable + 5056,
    bishopAttacksTable + 2720, bishopAttacksTable + 864,  bishopAttacksTable + 1248,
    bishopAttacksTable + 1632, bishopAttacksTable + 2272, bishopAttacksTable + 4896,
    bishopAttacksTable + 5184};

constexpr U64 rookMagic[64] = {
    0x0080001020400080ull, 0x0040001000200040ull, 0x0080081000200080ull, 0x0080040800100080ull,
    0x0080020400080080ull, 0x0080010200040080ull, 0x0080008001000200ull, 0x0080002040800100ull,
    0x0000800020400080ull, 0x0000400020005000ull, 0x0000801000200080ull, 0x0000800800100080ull,
    0x0000800400080080ull, 0x0000800200040080ull, 0x0000800100020080ull, 0x0000800040800100ull,
    0x0000208000400080ull, 0x0000404000201000ull, 0x0000808010002000ull, 0x0000808008001000ull,
    0x0000808004000800ull, 0x0000808002000400ull, 0x0000010100020004ull, 0x0000020000408104ull,
    0x0000208080004000ull, 0x0000200040005000ull, 0x0000100080200080ull, 0x0000080080100080ull,
    0x0000040080080080ull, 0x0000020080040080ull, 0x0000010080800200ull, 0x0000800080004100ull,
    0x0000204000800080ull, 0x0000200040401000ull, 0x0000100080802000ull, 0x0000080080801000ull,
    0x0000040080800800ull, 0x0000020080800400ull, 0x0000020001010004ull, 0x0000800040800100ull,
    0x0000204000808000ull, 0x0000200040008080ull, 0x0000100020008080ull, 0x0000080010008080ull,
    0x0000040008008080ull, 0x0000020004008080ull, 0x0000010002008080ull, 0x0000004081020004ull,
    0x0000204000800080ull, 0x0000200040008080ull, 0x0000100020008080ull, 0x0000080010008080ull,
    0x0000040008008080ull, 0x0000020004008080ull, 0x0000800100020080ull, 0x0000800041000080ull,
    0x00FFFCDDFCED714Aull, 0x007FFCDDFCED714Aull, 0x003FFFCDFFD88096ull, 0x0000040810002101ull,
    0x0001000204080011ull, 0x0001000204000801ull, 0x0001000082000401ull, 0x0001FFFAABFAD1A2ull};

constexpr U64 bishopMagic[64] = {
    0x0002020202020200ull, 0x0002020202020000ull, 0x0004010202000000ull, 0x0004040080000000ull,
    0x0001104000000000ull, 0x0000821040000000ull, 0x0000410410400000ull, 0x0000104104104000ull,
    0x0000040404040400ull, 0x0000020202020200ull, 0x0000040102020000ull, 0x0000040400800000ull,
    0x0000011040000000ull, 0x0000008210400000ull, 0x0000004104104000ull, 0x0000002082082000ull,
    0x0004000808080800ull, 0x0002000404040400ull, 0x0001000202020200ull, 0x0000800802004000ull,
    0x0000800400A00000ull, 0x0000200100884000ull, 0x0000400082082000ull, 0x0000200041041000ull,
    0x0002080010101000ull, 0x0001040008080800ull, 0x0000208004010400ull, 0x0000404004010200ull,
    0x0000840000802000ull, 0x0000404002011000ull, 0x0000808001041000ull, 0x0000404000820800ull,
    0x0001041000202000ull, 0x0000820800101000ull, 0x0000104400080800ull, 0x0000020080080080ull,
    0x0000404040040100ull, 0x0000808100020100ull, 0x0001010100020800ull, 0x0000808080010400ull,
    0x0000820820004000ull, 0x0000410410002000ull, 0x0000082088001000ull, 0x0000002011000800ull,
    0x0000080100400400ull, 0x0001010101000200ull, 0x0002020202000400ull, 0x0001010101000200ull,
    0x0000410410400000ull, 0x0000208208200000ull, 0x0000002084100000ull, 0x0000000020880000ull,
    0x0000001002020000ull, 0x0000040408020000ull, 0x0004040404040000ull, 0x0002020202020000ull,
    0x0000104104104000ull, 0x0000002082082000ull, 0x0000000020841000ull, 0x0000000000208800ull,
    0x0000000010020200ull, 0x0000000404080200ull, 0x0000040404040400ull, 0x0002020202020200ull};

constexpr U64 rookMask[64] = {
    0x000101010101017Eull, 0x000202020202027Cull, 0x000404040404047Aull, 0x0008080808080876ull,
    0x001010101010106Eull, 0x002020202020205Eull, 0x004040404040403Eull, 0x008080808080807Eull,
    0x0001010101017E00ull, 0x0002020202027C00ull, 0x0004040404047A00ull, 0x0008080808087600ull,
    0x0010101010106E00ull, 0x0020202020205E00ull, 0x0040404040403E00ull, 0x0080808080807E00ull,
    0x00010101017E0100ull, 0x00020202027C0200ull, 0x00040404047A0400ull, 0x0008080808760800ull,
    0x00101010106E1000ull, 0x00202020205E2000ull, 0x00404040403E4000ull, 0x00808080807E8000ull,
    0x000101017E010100ull, 0x000202027C020200ull, 0x000404047A040400ull, 0x0008080876080800ull,
    0x001010106E101000ull, 0x002020205E202000ull, 0x004040403E404000ull, 0x008080807E808000ull,
    0x0001017E01010100ull, 0x0002027C02020200ull, 0x0004047A04040400ull, 0x0008087608080800ull,
    0x0010106E10101000ull, 0x0020205E20202000ull, 0x0040403E40404000ull, 0x0080807E80808000ull,
    0x00017E0101010100ull, 0x00027C0202020200ull, 0x00047A0404040400ull, 0x0008760808080800ull,
    0x00106E1010101000ull, 0x00205E2020202000ull, 0x00403E4040404000ull, 0x00807E8080808000ull,
    0x007E010101010100ull, 0x007C020202020200ull, 0x007A040404040400ull, 0x0076080808080800ull,
    0x006E101010101000ull, 0x005E202020202000ull, 0x003E404040404000ull, 0x007E808080808000ull,
    0x7E01010101010100ull, 0x7C02020202020200ull, 0x7A04040404040400ull, 0x7608080808080800ull,
    0x6E10101010101000ull, 0x5E20202020202000ull, 0x3E40404040404000ull, 0x7E80808080808000ull};

constexpr U64 bishopMask[64] = {
    0x0040201008040200ull, 0x0000402010080400ull, 0x0000004020100A00ull, 0x0000000040221400ull,
    0x0000000002442800ull, 0x0000000204085000ull, 0x0000020408102000ull, 0x0002040810204000ull,
    0x0020100804020000ull, 0x0040201008040000ull, 0x00004020100A0000ull, 0x0000004022140000ull,
    0x0000000244280000ull, 0x0000020408500000ull, 0x0002040810200000ull, 0x0004081020400000ull,
    0x0010080402000200ull, 0x0020100804000400ull, 0x004020100A000A00ull, 0x0000402214001400ull,
    0x0000024428002800ull, 0x0002040850005000ull, 0x0004081020002000ull, 0x0008102040004000ull,
    0x0008040200020400ull, 0x0010080400040800ull, 0x0020100A000A1000ull, 0x0040221400142200ull,
    0x0002442800284400ull, 0x0004085000500800ull, 0x0008102000201000ull, 0x0010204000402000ull,
    0x0004020002040800ull, 0x0008040004081000ull, 0x00100A000A102000ull, 0x0022140014224000ull,
    0x0044280028440200ull, 0x0008500050080400ull, 0x0010200020100800ull, 0x0020400040201000ull,
    0x0002000204081000ull, 0x0004000408102000ull, 0x000A000A10204000ull, 0x0014001422400000ull,
    0x0028002844020000ull, 0x0050005008040200ull, 0x0020002010080400ull, 0x0040004020100800ull,
    0x0000020408102000ull, 0x0000040810204000ull, 0x00000A1020400000ull, 0x0000142240000000ull,
    0x0000284402000000ull, 0x0000500804020000ull, 0x0000201008040200ull, 0x0000402010080400ull,
    0x0002040810204000ull, 0x0004081020400000ull, 0x000A102040000000ull, 0x0014224000000000ull,
    0x0028440200000000ull, 0x0050080402000000ull, 0x0020100804020000ull, 0x0040201008040200ull};

constexpr int rookShift[64] = {52, 53, 53, 53, 53, 53, 53, 52, 53, 54, 54, 54, 54, 54, 54, 53,
                               53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53,
                               53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53,
                               53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 53, 53, 53, 53, 53};

constexpr int bishopShift[64] = {58, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 59, 59,
                                 59, 59, 57, 57, 57, 57, 59, 59, 59, 59, 57, 55, 55, 57, 59, 59,
                                 59, 59, 57, 55, 55, 57, 59, 59, 59, 59, 57, 57, 57, 57, 59, 59,
                                 59, 59, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 58};

}  // namespace MagicTables
