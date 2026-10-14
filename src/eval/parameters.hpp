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

inline constexpr TaperedScore pawn   = {100, 198};
inline constexpr TaperedScore knight = {603, 677};
inline constexpr TaperedScore bishop = {615, 707};
inline constexpr TaperedScore rook   = {901, 1082};
inline constexpr TaperedScore queen  = {2073, 2080};

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
              10,  22,   8,  14,  30,  10,  23,  -7,
               6,  17,  11,  34,  41,   9,  11,  -1,
               4,  12,  45,  81,  86,  37,  11,   1,
               6,  14,  48,  77, 100,  53,  11,  14,
              31,  15,  62,  72,  62,  66, -13,  -3,
              23,  -4,  21,  25,  12,  26,   7,  25,
               0,   0,   0,   0,   0,   0,   0,   0,
        },
        // Pawn endgame bonuses
        {
               0,   0,   0,   0,   0,   0,   0,   0,
              -3, -17,   8,  -7, -17,  -4, -15,   5,
              11,  -4,  -4, -13, -15,  -2,  -7,  11,
              21,  22, -11, -25, -28, -12,  15,  23,
              48,  30, -19, -30, -41,  -5,  27,  44,
              74,  85,  25, -47, -39,   2,  87,  79,
              63,  12,  18, -19,   4,  42,  39,  61,
               0,   0,   0,   0,   0,   0,   0,   0,
        }
    },
    {
        // Knight midgame bonuses
        {
            -139, -69, -43,  -2,  -2, -43, -69,-139,
             -21, -21, -17, -12, -12, -17, -21, -21,
             -38, -13,   5,  25,  25,   5, -13, -38,
               1,  28,  30,  24,  24,  30,  28,   1,
              24,  24,  53,  52,  52,  53,  24,  24,
              -8,  44,  50,  97,  97,  50,  44,  -8,
             -22, -45,  63,  47,  47,  63, -45, -22,
            -217, -66, -62, -24, -24, -62, -66,-217,
        },
        // Knight endgame bonuses
        {
             -70, -30, -26, -25, -25, -26, -30, -70,
             -49, -32, -27,  -8,  -8, -27, -32, -49,
              -9,  -6,  -6,  12,  12,  -6,  -6,  -9,
             -20, -14,   7,  10,  10,   7, -14, -20,
             -17,   0,  -6,   6,   6,  -6,   0, -17,
             -36, -29, -16, -15, -15, -16, -29, -36,
             -23, -16, -30, -11, -11, -30, -16, -23,
            -106, -55, -42,  -9,  -9, -42, -55,-106,
        }
    },
    {
        // Bishop midgame bonuses
        {
             -24,  36, -10,   5,   5, -10,  36, -24,
              31,   9,  37,   3,   3,  37,   9,  31,
              17,  43,   3,  20,  20,   3,  43,  17,
              24,   6,  15,  43,  43,  15,   6,  24,
              -3,  17,  31,  41,  41,  31,  17,  -3,
              45,  44,  20,  26,  26,  20,  44,  45,
             -23, -49,   2,   5,   5,   2, -49, -23,
             -41,  11, -24, -37, -37, -24,  11, -41,
        },
        // Bishop endgame bonuses
        {
             -56, -27,  -4,  -7,  -7,  -4, -27, -56,
             -31, -44, -20,   1,   1, -20, -44, -31,
             -14,  -8, -18,  11,  11, -18,  -8, -14,
             -18, -12,   2,   5,   5,   2, -12, -18,
              -7,   7, -16,  16,  16, -16,   7,  -7,
             -14,  26, -20,  -1,  -1, -20,  26, -14,
             -13, -22,  11,   6,   6,  11, -22, -13,
             -33,  -5, -37, -14, -14, -37,  -5, -33,
        }
    },
    {
        // Rook midgame bonuses
        {
             -25, -12,  -5,   2,   2,  -5, -12, -25,
             -54, -27,  -8, -29, -29,  -8, -27, -54,
             -49, -20, -33, -29, -29, -33, -20, -49,
             -35, -16, -24, -30, -30, -24, -16, -35,
               8,   6,  14,   8,   8,  14,   6,   8,
              -3,  24,  16,  51,  51,  16,  24,  -3,
              14, -21,  21,  48,  48,  21, -21,  14,
             -15, -11,  -6,   9,   9,  -6, -11, -15,
        },
        // Rook endgame bonuses
        {
              -7, -22, -16, -24, -24, -16, -22,  -7,
              -6, -22, -23,  -4,  -4, -23, -22,  -6,
               4,  -6,  -9, -18, -18,  -9,  -6,   4,
              15,  23,   9,  11,  11,   9,  23,  15,
              18,  24,  12,  15,  15,  12,  24,  18,
              14,  12,  12,   6,   6,  12,  12,  14,
               9,  29,  31,  24,  24,  31,  29,   9,
              14,   6,  28,  13,  13,  28,   6,  14,
        }
    },
    {
        // Queen midgame bonuses
        {
               5,  10,  -8,   3,   3,  -8,  10,   5,
               9,  13,  16,  12,  12,  16,  13,   9,
              11,   3,   8, -13, -13,   8,   3,  11,
               8,   6, -15, -19, -19, -15,   6,   8,
              36,  -7,  -7, -35, -35,  -7,  -7,  36,
              59,  42,   1,  16,  16,   1,  42,  59,
               4, -86,   2,   5,   5,   2, -86,   4,
             -33,   1,  -3,   3,   3,  -3,   1, -33,
        },
        // Queen endgame bonuses
        {
             -64, -74, -82, -21, -21, -82, -74, -64,
             -57, -59, -58, -41, -41, -58, -59, -57,
             -24, -21, -15,   4,   4, -15, -21, -24,
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
             235, 264, 231, 150, 150, 231, 264, 235,
             260, 262, 233, 226, 226, 233, 262, 260,
             172, 228, 179, 150, 150, 179, 228, 172,
             142, 162, 121, 107, 107, 121, 162, 142,
             137, 154,  99,  70,  70,  99, 154, 137,
             108, 126,  82,  36,  36,  82, 126, 108,
              71, 111,  62,  35,  35,  62, 111,  71,
              47,  72,  36,  -1,  -1,  36,  72,  47,
        },
        // King endgame bonuses
        {
             -31,  36,  84, 103, 103,  84,  36, -31,
              48,  83, 134, 152, 152, 134,  83,  48,
             102, 127, 179, 210, 210, 179, 127, 102,
             120, 159, 207, 221, 221, 207, 159, 120,
             124, 197, 224, 250, 250, 224, 197, 124,
             119, 201, 238, 210, 210, 238, 201, 119,
              39, 157, 147, 146, 146, 147, 157,  39,
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

inline constexpr TaperedScore isolated_pawn = {-12, -21};
inline constexpr TaperedScore backward_pawn = {-24, -17};
inline constexpr TaperedScore doubled_pawn  = {5, -30};
inline constexpr TaperedScore connected_pawn_link = {31, 8};
inline constexpr TaperedScore passed_pawn[] = {
    {0, 0}, {0, 0}, {-7, 15}, {-8, 69}, {37, 120}, {99, 211}, {195, 292}, {0, 0}};
inline constexpr TaperedScore reachable_outpost       = {18, 23};
inline constexpr TaperedScore bishop_outpost          = {49, -20};
inline constexpr TaperedScore knight_outpost          = {59, 25};
inline constexpr TaperedScore minor_pawn_shield       = {14, 6};
inline constexpr TaperedScore bishop_long_diagonal    = {28, 31};
inline constexpr TaperedScore bishop_pair             = {50, 109};
inline constexpr TaperedScore bishop_blockers         = {-1, -7};
inline constexpr TaperedScore rook_closed_file        = {-7, -32};
inline constexpr TaperedScore king_zone_xray_attack   = {17, 13};
inline constexpr TaperedScore queen_discovered_attack = {-53, 52};

// bonus for rook on open files: [0 = semi-open, 1 = fully open]
inline constexpr TaperedScore rook_open_file[] = {{28, 2}, {58, -5}};

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
    TaperedScore::Zero, TaperedScore::Zero, {-68, -58}, {-65, -53}, {-38, -71}, {-89, -4}};

// Piece mobility scores (index = # of legal moves)
inline constexpr TaperedScore knight_mob[] = {
    {-72, -84},
    {-70, -86},
    {-45, -27},
    {-17, -21},
    {-11, -3},
    {0, 5},
    {12, 12},
    {27, 7},
    {39, -1},
};

inline constexpr TaperedScore bishop_mob[] = {
    {-100, -33},
    {-39, -69},
    {-24, -5},
    {-3, -8},
    {10, 14},
    {28, 20},
    {41, 23},
    {40, 36},
    {48, 31},
    {53, 35},
    {60, 21},
    {89, 18},
    {78, 31},
    {57, 29},
};

inline constexpr TaperedScore rook_mob[] = {
    {-155, -99},
    {-74, -13},
    {-54, 31},
    {-50, 68},
    {-52, 78},
    {-45, 85},
    {-41, 107},
    {-39, 110},
    {-28, 119},
    {-22, 123},
    {-10, 125},
    {32, 104},
    {24, 104},
    {20, 103},
    {47, 92},
};

inline constexpr TaperedScore queen_mob[] = {
    {-20, -32}, {-13, -14}, {-16, -2}, {-28, 3},  {-21, 12}, {-7, 12},  {8, 19},
    {22, 9},    {25, 37},   {23, 56},  {38, 51},  {44, 64},  {44, 78},  {50, 75},
    {51, 92},   {56, 106},  {61, 99},  {54, 104}, {69, 98},  {62, 105}, {66, 106},
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
