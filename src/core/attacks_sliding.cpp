/**
 * Source file for PEXT and magic sliding attack generation.
 *
 * This file uses magic numbers and related constants from Pradyumna Kannan's work.
 * The implementation code including move generation functions and
 * initialization routines are original to Latrunculi.
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

#include "core/attacks_sliding.hpp"

#include <array>
#include <cstdint>
#include <utility>

#include "core/bitboard.hpp"

namespace attacks::sliding {

namespace {

std::array<Bitboard, 102400> rook_storage{};
std::array<Bitboard, 5248> bishop_storage{};

} // namespace

namespace detail {

// Per-square slices fit every blocker subset, including the PEXT index range.
const SquareTable<Bitboard*> rook_attacks = {
    rook_storage.data() + 86016, rook_storage.data() + 73728, rook_storage.data() + 36864,
    rook_storage.data() + 43008, rook_storage.data() + 47104, rook_storage.data() + 51200,
    rook_storage.data() + 77824, rook_storage.data() + 94208, rook_storage.data() + 69632,
    rook_storage.data() + 32768, rook_storage.data() + 38912, rook_storage.data() + 10240,
    rook_storage.data() + 14336, rook_storage.data() + 53248, rook_storage.data() + 57344,
    rook_storage.data() + 81920, rook_storage.data() + 24576, rook_storage.data() + 33792,
    rook_storage.data() + 6144,  rook_storage.data() + 11264, rook_storage.data() + 15360,
    rook_storage.data() + 18432, rook_storage.data() + 58368, rook_storage.data() + 61440,
    rook_storage.data() + 26624, rook_storage.data() + 4096,  rook_storage.data() + 7168,
    rook_storage.data() + 0,     rook_storage.data() + 2048,  rook_storage.data() + 19456,
    rook_storage.data() + 22528, rook_storage.data() + 63488, rook_storage.data() + 28672,
    rook_storage.data() + 5120,  rook_storage.data() + 8192,  rook_storage.data() + 1024,
    rook_storage.data() + 3072,  rook_storage.data() + 20480, rook_storage.data() + 23552,
    rook_storage.data() + 65536, rook_storage.data() + 30720, rook_storage.data() + 34816,
    rook_storage.data() + 9216,  rook_storage.data() + 12288, rook_storage.data() + 16384,
    rook_storage.data() + 21504, rook_storage.data() + 59392, rook_storage.data() + 67584,
    rook_storage.data() + 71680, rook_storage.data() + 35840, rook_storage.data() + 39936,
    rook_storage.data() + 13312, rook_storage.data() + 17408, rook_storage.data() + 54272,
    rook_storage.data() + 60416, rook_storage.data() + 83968, rook_storage.data() + 90112,
    rook_storage.data() + 75776, rook_storage.data() + 40960, rook_storage.data() + 45056,
    rook_storage.data() + 49152, rook_storage.data() + 55296, rook_storage.data() + 79872,
    rook_storage.data() + 98304};

const SquareTable<Bitboard*> bishop_attacks = {
    bishop_storage.data() + 4992, bishop_storage.data() + 2624, bishop_storage.data() + 256,
    bishop_storage.data() + 896,  bishop_storage.data() + 1280, bishop_storage.data() + 1664,
    bishop_storage.data() + 4800, bishop_storage.data() + 5120, bishop_storage.data() + 2560,
    bishop_storage.data() + 2656, bishop_storage.data() + 288,  bishop_storage.data() + 928,
    bishop_storage.data() + 1312, bishop_storage.data() + 1696, bishop_storage.data() + 4832,
    bishop_storage.data() + 4928, bishop_storage.data() + 0,    bishop_storage.data() + 128,
    bishop_storage.data() + 320,  bishop_storage.data() + 960,  bishop_storage.data() + 1344,
    bishop_storage.data() + 1728, bishop_storage.data() + 2304, bishop_storage.data() + 2432,
    bishop_storage.data() + 32,   bishop_storage.data() + 160,  bishop_storage.data() + 448,
    bishop_storage.data() + 2752, bishop_storage.data() + 3776, bishop_storage.data() + 1856,
    bishop_storage.data() + 2336, bishop_storage.data() + 2464, bishop_storage.data() + 64,
    bishop_storage.data() + 192,  bishop_storage.data() + 576,  bishop_storage.data() + 3264,
    bishop_storage.data() + 4288, bishop_storage.data() + 1984, bishop_storage.data() + 2368,
    bishop_storage.data() + 2496, bishop_storage.data() + 96,   bishop_storage.data() + 224,
    bishop_storage.data() + 704,  bishop_storage.data() + 1088, bishop_storage.data() + 1472,
    bishop_storage.data() + 2112, bishop_storage.data() + 2400, bishop_storage.data() + 2528,
    bishop_storage.data() + 2592, bishop_storage.data() + 2688, bishop_storage.data() + 832,
    bishop_storage.data() + 1216, bishop_storage.data() + 1600, bishop_storage.data() + 2240,
    bishop_storage.data() + 4864, bishop_storage.data() + 4960, bishop_storage.data() + 5056,
    bishop_storage.data() + 2720, bishop_storage.data() + 864,  bishop_storage.data() + 1248,
    bishop_storage.data() + 1632, bishop_storage.data() + 2272, bishop_storage.data() + 4896,
    bishop_storage.data() + 5184};

// Per-square multipliers for hashing relevant blockers.
const SquareTable<std::uint64_t> rook_magic = {
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

const SquareTable<std::uint64_t> bishop_magic = {
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

// Relevant blocker masks. Edge blockers are still attacked, with no farther
// squares behind them, so they do not affect the lookup key.
const SquareTable<Bitboard> rook_masks = {
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

const SquareTable<Bitboard> bishop_masks = {
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

// Right shifts that compact each magic product into its table slice.
const SquareTable<int> rook_shifts = {
    52, 53, 53, 53, 53, 53, 53, 52, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54,
    54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54,
    54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 53, 53, 53, 53, 53};

const SquareTable<int> bishop_shifts = {
    58, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 57, 57, 57, 57,
    59, 59, 59, 59, 57, 55, 55, 57, 59, 59, 59, 59, 57, 55, 55, 57, 59, 59, 59, 59, 57, 57,
    57, 57, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 58};

} // namespace detail

namespace {

// File/rank delta for one slider ray.
using SliderDirection = std::pair<int, int>;
using SliderDirections = std::array<SliderDirection, 4>;

// Slow slider walk used only while building lookup tables.
Bitboard generate_slider_moves(Square from, Bitboard occupied, const SliderDirections& directions) {
    Bitboard attacks = 0ULL;

    for (const auto& [file_delta, rank_delta] : directions) {

        // Start one step away; the source square is never attacked.
        int file = square::file_of(from) + file_delta;
        int rank = square::rank_of(from) + rank_delta;

        while (0 <= rank && rank < 8 && 0 <= file && file < 8) {
            const Bitboard to_bb = bb::set(square::make(File(file), Rank(rank)));

            attacks |= to_bb;
            if (occupied & to_bb)
                break;

            file += file_delta;
            rank += rank_delta;
        }
    }

    return attacks;
}

// Enumerate blocker subsets with unsigned wraparound; no temporary allocations.
template <PieceType P>
void init_slider(const SliderDirections& directions) {
    const auto& masks = P == BISHOP ? detail::bishop_masks : detail::rook_masks;
    const auto& moves = P == BISHOP ? detail::bishop_attacks : detail::rook_attacks;

    for (Square sq = A1; sq < INVALID; ++sq) {
        const Bitboard mask = masks[sq];
        Bitboard occupied = 0;
        do {
            moves[sq][detail::occupancy_index<P>(sq, occupied)] =
                generate_slider_moves(sq, occupied, directions);
            occupied = (occupied - mask) & mask;
        } while (occupied);
    }
}

} // namespace

void init() {
    // Directions are file/rank deltas from the source square.
    constexpr SliderDirections bishop_dirs = {{{-1, 1}, {1, 1}, {-1, -1}, {1, -1}}};
    constexpr SliderDirections rook_dirs = {{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};

    init_slider<BISHOP>(bishop_dirs);
    init_slider<ROOK>(rook_dirs);
}

} // namespace attacks::sliding
