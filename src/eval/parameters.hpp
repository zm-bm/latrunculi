#pragma once

#include <cassert>
#include <cstdint>
#include <utility>

#include "core/piece.hpp"
#include "core/square.hpp"
#include "eval/tapered_score.hpp"

namespace eval {

enum class Phase : std::uint8_t {
    Midgame = 0,
    Endgame,
    Count,
};

inline constexpr int tempo_bonus    = 20;
inline constexpr int scale_limit    = 64;
inline constexpr int scale_base     = 48;
inline constexpr int scale_per_pawn = 4;
inline constexpr int phase_limit    = 128;

inline constexpr TaperedScore pawn   = {100, 199};
inline constexpr TaperedScore knight = {602, 679};
inline constexpr TaperedScore bishop = {617, 705};
inline constexpr TaperedScore rook   = {901, 1086};
inline constexpr TaperedScore queen  = {2074, 2080};

inline constexpr int material_mg = 4 * knight.mg + 4 * bishop.mg + 4 * rook.mg + 2 * queen.mg;
inline constexpr int material_eg = 0;

namespace masks {

inline constexpr Bitboard dark_squares  = 0xAA55AA55AA55AA55ull;
inline constexpr Bitboard light_squares = 0x55AA55AA55AA55AAull;
inline constexpr Bitboard center_files =
    (bb::file(FILE3) | bb::file(FILE4) | bb::file(FILE5) | bb::file(FILE6)) & ~bb::rank(RANK8);
inline constexpr Bitboard center_squares = bb::set(D4, E4, D5, E5);
inline constexpr Bitboard w_outposts     = bb::rank(RANK4) | bb::rank(RANK5) | bb::rank(RANK6);
inline constexpr Bitboard b_outposts     = bb::rank(RANK3) | bb::rank(RANK4) | bb::rank(RANK5);

} // namespace masks

inline constexpr TaperedScore piece_values[N_PIECETYPES][N_COLORS] = {
    {TaperedScore::Zero, TaperedScore::Zero},
    {-pawn, pawn},
    {-knight, knight},
    {-bishop, bishop},
    {-rook, rook},
    {-queen, queen},
    {TaperedScore::Zero, TaperedScore::Zero},
};

inline constexpr int piece_squares[piece_slots][std::to_underlying(Phase::Count)][N_SQUARES] = {
    // clang-format off
    {
        // Pawn midgame bonuses
        {
               0,   0,   0,   0,   0,   0,   0,   0,
               9,  23,   8,  16,  28,   9,  20,  -5,
               7,   8,   8,  36,  42,  11,   6,  -3,
               4,  14,  40,  80,  82,  38,  12,   1,
               7,  17,  56,  78,  96,  53,  14,  14,
              30,  16,  63,  73,  63,  65, -12,  -2,
              23,  -4,  21,  26,  12,  26,   7,  25,
               0,   0,   0,   0,   0,   0,   0,   0,
        },
        // Pawn endgame bonuses
        {
               0,   0,   0,   0,   0,   0,   0,   0,
               1, -21,   8,  -6, -18,  -2, -15,   5,
              11,  -5,  -4, -10, -13,  -4,  -4,  13,
              19,  19, -11, -26, -30, -12,  15,  24,
              47,  31, -15, -30, -41,  -6,  28,  43,
              74,  86,  25, -46, -40,   2,  88,  79,
              64,  13,  18, -19,   4,  42,  39,  61,
               0,   0,   0,   0,   0,   0,   0,   0,
        }
    },
    {
        // Knight midgame bonuses
        {
            -139, -69, -42,  -2,  -2, -42, -69,-139,
             -22, -21, -16, -11, -11, -16, -21, -22,
             -39, -11,   5,  25,  25,   5, -11, -39,
               2,  28,  30,  25,  25,  30,  28,   2,
              25,  22,  52,  52,  52,  52,  22,  25,
              -8,  44,  50,  97,  97,  50,  44,  -8,
             -21, -45,  63,  47,  47,  63, -45, -21,
            -217, -66, -62, -24, -24, -62, -66,-217,
        },
        // Knight endgame bonuses
        {
             -70, -30, -26, -25, -25, -26, -30, -70,
             -49, -32, -27,  -8,  -8, -27, -32, -49,
             -10,  -6,  -6,  12,  12,  -6,  -6, -10,
             -20, -14,   7,  10,  10,   7, -14, -20,
             -17,  -1,  -6,   6,   6,  -6,  -1, -17,
             -36, -29, -16, -15, -15, -16, -29, -36,
             -23, -16, -30, -11, -11, -30, -16, -23,
            -106, -55, -42,  -9,  -9, -42, -55,-106,
        }
    },
    {
        // Bishop midgame bonuses
        {
             -24,  36,  -9,   5,   5,  -9,  36, -24,
              31,  11,  36,   3,   3,  36,  11,  31,
              19,  40,   5,  21,  21,   5,  40,  19,
              23,   6,  14,  43,  43,  14,   6,  23,
              -4,  17,  31,  41,  41,  31,  17,  -4,
              45,  44,  20,  26,  26,  20,  44,  45,
             -23, -49,   2,   5,   5,   2, -49, -23,
             -41,  11, -24, -37, -37, -24,  11, -41,
        },
        // Bishop endgame bonuses
        {
             -56, -27,  -3,  -7,  -7,  -3, -27, -56,
             -31, -44, -21,   1,   1, -21, -44, -31,
             -14,  -9, -18,  11,  11, -18,  -9, -14,
             -18, -12,   2,   6,   6,   2, -12, -18,
              -7,   8, -15,  15,  15, -15,   8,  -7,
             -14,  25, -20,  -1,  -1, -20,  25, -14,
             -13, -22,  11,   6,   6,  11, -22, -13,
             -33,  -5, -37, -14, -14, -37,  -5, -33,
        }
    },
    {
        // Rook midgame bonuses
        {
             -25, -12,  -6,   4,   4,  -6, -12, -25,
             -54, -26,  -8, -29, -29,  -8, -26, -54,
             -49, -19, -33, -29, -29, -33, -19, -49,
             -35, -16, -25, -30, -30, -25, -16, -35,
               7,   6,  14,   8,   8,  14,   6,   7,
              -3,  25,  16,  51,  51,  16,  25,  -3,
              14, -21,  20,  48,  48,  20, -21,  14,
             -15, -11,  -6,   9,   9,  -6, -11, -15,
        },
        // Rook endgame bonuses
        {
              -7, -22, -16, -25, -25, -16, -22,  -7,
              -6, -22, -23,  -4,  -4, -23, -22,  -6,
               4,  -6,  -9, -18, -18,  -9,  -6,   4,
              15,  23,   9,  11,  11,   9,  23,  15,
              18,  24,  12,  15,  15,  12,  24,  18,
              14,  13,  12,   6,   6,  12,  13,  14,
              10,  30,  31,  24,  24,  31,  30,  10,
              14,   7,  28,  13,  13,  28,   7,  14,
        }
    },
    {
        // Queen midgame bonuses
        {
               4,  10,  -7,   3,   3,  -7,  10,   4,
               9,  12,  16,  12,  12,  16,  12,   9,
              12,   6,   9, -14, -14,   9,   6,  12,
               9,   5, -14, -20, -20, -14,   5,   9,
              35,  -7,  -7, -36, -36,  -7,  -7,  35,
              59,  42,   1,  16,  16,   1,  42,  59,
               4, -86,   2,   5,   5,   2, -86,   4,
             -33,   1,  -3,   3,   3,  -3,   1, -33,
        },
        // Queen endgame bonuses
        {
             -64, -74, -82, -21, -21, -82, -74, -64,
             -57, -60, -59, -41, -41, -59, -60, -57,
             -24, -20, -15,   3,   3, -15, -20, -24,
              -6,  15,  16,  24,  24,  16,  15,  -6,
             -19,  34,  18,  42,  42,  18,  34, -19,
             -16,   3,  22,  24,  24,  22,   3, -16,
             -16,  10,  -5,  26,  26,  -5,  10, -16,
             -52, -37, -35, -26, -26, -35, -37, -52,
        }
    },
    {
        // King midgame bonuses
        {
             234, 264, 229, 151, 151, 229, 264, 234,
             260, 262, 233, 226, 226, 233, 262, 260,
             172, 228, 180, 150, 150, 180, 228, 172,
             142, 162, 121, 107, 107, 121, 162, 142,
             137, 154,  99,  70,  70,  99, 154, 137,
             108, 126,  82,  36,  36,  82, 126, 108,
              71, 111,  62,  35,  35,  62, 111,  71,
              47,  72,  36,  -1,  -1,  36,  72,  47,
        },
        // King endgame bonuses
        {
             -32,  36,  82, 104, 104,  82,  36, -32,
              48,  83, 134, 152, 152, 134,  83,  48,
             102, 128, 180, 209, 209, 180, 128, 102,
             120, 160, 208, 222, 222, 208, 160, 120,
             124, 198, 224, 250, 250, 224, 198, 124,
             119, 201, 238, 211, 211, 238, 201, 119,
              39, 157, 148, 146, 146, 148, 157,  39,
               9,  47,  59,  63,  63,  59,  47,   9,
        }
    }
    // clang-format on
};

constexpr TaperedScore piece(PieceType pt, Color c = WHITE) {
    assert(pt <= KING);
    return piece_values[pt][c];
}

constexpr TaperedScore piece_sq(PieceType pt, Color c, Square sq) {
    assert(is_piece_type(pt));
    Square       relative = square::relative(sq, c);
    TaperedScore score    = {
           .mg = piece_squares[piece_slot(pt)][std::to_underlying(Phase::Midgame)][relative],
           .eg = piece_squares[piece_slot(pt)][std::to_underlying(Phase::Endgame)][relative]};

    return (score * c * 2) - score;
}

inline constexpr TaperedScore isolated_pawn = {-12, -17};
inline constexpr TaperedScore backward_pawn = {-20, -18};
inline constexpr TaperedScore doubled_pawn  = {2, -34};
inline constexpr TaperedScore connected_pawn_link = {31, 9};
inline constexpr TaperedScore passed_pawn[] = {
    {0, 0}, {1, 1}, {-9, 15}, {-8, 69}, {36, 120}, {102, 214}, {196, 293}, {0, 0}};
inline constexpr TaperedScore reachable_outpost       = {18, 23};
inline constexpr TaperedScore bishop_outpost          = {51, -20};
inline constexpr TaperedScore knight_outpost          = {61, 26};
inline constexpr TaperedScore minor_pawn_shield       = {13, 6};
inline constexpr TaperedScore bishop_long_diagonal    = {33, 31};
inline constexpr TaperedScore bishop_pair             = {49, 109};
inline constexpr TaperedScore bishop_blockers         = {-2, -7};
inline constexpr TaperedScore rook_closed_file        = {-3, -31};
inline constexpr TaperedScore king_zone_xray_attack   = {17, 13};
inline constexpr TaperedScore queen_discovered_attack = {-54, 52};

// bonus for rook on open files: [0 = semi-open, 1 = fully open]
inline constexpr TaperedScore rook_open_file[] = {{26, 2}, {61, -4}};

// shelter bonus for friendly pawn rank [index = pawn rank, 0 = no pawn]
inline constexpr TaperedScore pawn_shelter[] = {
    {-30, 0}, {60, 0}, {35, 0}, {-20, 0}, {-5, 0}, {-20, 0}, {-80, 0}};

// Pawn storm penalty by pawn rank:
// [0 = unblocked, 1 = blocked][index = pawn rank, 0 = no pawn)]
inline constexpr TaperedScore pawn_storm[][7] = {
    {{0, 0}, {-20, 0}, {-120, 0}, {-60, 0}, {-45, 0}, {-20, 0}, {-10, 0}},
    {{0, 0}, {0, 0}, {-60, -60}, {0, -20}, {5, -15}, {10, -10}, {15, -5}}};

// score for king on open/closed files: [friendly file][enemy file] (0 = closed, 1 = open)
inline constexpr TaperedScore king_open_file[][2] = {
    {{20, -10}, {10, 5}},
    {{0, 0}, {-10, 5}},
};

// Bonus for king based on file [index = king file]
inline constexpr TaperedScore king_file[] = {
    {20, 0}, {5, 0}, {-15, 0}, {-30, 0}, {-30, 0}, {-15, 0}, {5, 0}, {20, 0}};

// Penalty for potentially hanging piece [index = piece type]
inline constexpr TaperedScore weak_piece[] = {
    TaperedScore::Zero, TaperedScore::Zero, {-67, -58}, {-65, -52}, {-38, -71}, {-88, -4}};

// Piece mobility scores (index = # of legal moves)
inline constexpr TaperedScore knight_mob[] = {
    {-73, -84},
    {-72, -87},
    {-44, -27},
    {-19, -21},
    {-9, -5},
    {-3, 5},
    {12, 12},
    {27, 10},
    {37, -3},
};

inline constexpr TaperedScore bishop_mob[] = {
    {-101, -34},
    {-39, -69},
    {-27, -6},
    {0, -9},
    {10, 14},
    {26, 21},
    {39, 24},
    {40, 36},
    {49, 33},
    {52, 35},
    {60, 21},
    {89, 17},
    {78, 31},
    {57, 29},
};

inline constexpr TaperedScore rook_mob[] = {
    {-155, -99},
    {-75, -13},
    {-50, 31},
    {-52, 68},
    {-49, 79},
    {-46, 86},
    {-42, 105},
    {-39, 113},
    {-28, 118},
    {-23, 123},
    {-10, 124},
    {32, 104},
    {24, 105},
    {21, 104},
    {47, 93},
};

inline constexpr TaperedScore queen_mob[] = {
    {-20, -32}, {-13, -14}, {-16, -2}, {-29, 3},  {-21, 12}, {-8, 12},  {9, 19},
    {20, 9},    {24, 37},   {27, 57},  {38, 52},  {44, 64},  {45, 78},  {50, 75},
    {52, 92},   {54, 105},  {60, 99},  {54, 104}, {68, 98},  {62, 105}, {66, 106},
    {77, 105},  {75, 113},  {68, 103}, {72, 116}, {72, 120}, {76, 124}, {80, 140},
};

inline constexpr const TaperedScore* mobility[] = {
    nullptr,
    nullptr,
    knight_mob,
    bishop_mob,
    rook_mob,
    queen_mob,
};

// danger values [index = piece type]
inline constexpr int king_zone_attack_danger[N_PIECETYPES] = {0, 0, 30, 22, 18, 5};
inline constexpr int safe_check_danger[N_PIECETYPES]       = {0, 0, 320, 240, 360, 280};
inline constexpr int unsafe_check_danger[N_PIECETYPES]     = {0, 0, 35, 30, 25, 5};

inline constexpr int pinned_piece_danger   = 30;
inline constexpr int weak_king_zone_danger = 80;

} // namespace eval
