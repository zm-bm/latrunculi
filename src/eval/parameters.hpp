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

inline constexpr TaperedScore pawn   = {100, 202};
inline constexpr TaperedScore knight = {576, 670};
inline constexpr TaperedScore bishop = {596, 698};
inline constexpr TaperedScore rook   = {901, 1058};
inline constexpr TaperedScore queen  = {2061, 2080};

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
               5,  37,   8,  28,  28,  16,  30,  -2,
               9,  30,  20,  54,  60,  19,  16,   2,
               2,   5,  34,  82,  83,  35,   2,  -7,
              -8,  -7,  59,  75,  83,  52,   8,  18,
              25,  20,  54,  75,  61,  63, -18,  -2,
              24,  -5,  22,  24,   9,  31,   9,  21,
               0,   0,   0,   0,   0,   0,   0,   0,
        },
        // Pawn endgame bonuses
        {
               0,   0,   0,   0,   0,   0,   0,   0,
               2, -26,   8, -12, -14,  -6, -16,   2,
               3,   2,   4, -23, -20,  -6,   0,   5,
               9,  14, -11, -35, -24,  -8,  20,  14,
              41,  20, -13, -34, -33,  -6,  26,  42,
              70,  88,  25, -34, -34,   1,  82,  73,
              58,   5,  23, -20,   9,  43,  39,  53,
               0,   0,   0,   0,   0,   0,   0,   0,
        }
    },
    {
        // Knight midgame bonuses
        {
            -142, -76, -52,   3,   3, -52, -76,-142,
             -14, -32, -15,  -4,  -4, -15, -32, -14,
             -49,   0,   5,  27,  27,   5,   0, -49,
              -3,  28,  30,  30,  30,  30,  28,  -3,
              19,  26,  48,  52,  52,  48,  26,  19,
             -12,  49,  53, 102, 102,  53,  49, -12,
             -25, -44,  64,  45,  45,  64, -44, -25,
            -216, -66, -63, -25, -25, -63, -66,-216,
        },
        // Knight endgame bonuses
        {
             -72, -30, -28, -22, -22, -28, -30, -72,
             -51, -36, -21,  -5,  -5, -21, -36, -51,
              -8,  -9,  -6,  15,  15,  -6,  -9,  -8,
             -18, -13,   5,  15,  15,   5, -13, -18,
             -20,   2,  -6,   5,   5,  -6,   2, -20,
             -37, -24, -16,  -9,  -9, -16, -24, -37,
             -22, -21, -28, -13, -13, -28, -21, -22,
            -106, -55, -45, -11, -11, -45, -55,-106,
        }
    },
    {
        // Bishop midgame bonuses
        {
             -28,  24, -15,   2,   2, -15,  24, -28,
              34,   2,  28,   3,   3,  28,   2,  34,
              18,  39,  14,  17,  17,  14,  39,  18,
              23,   0,  18,  42,  42,  18,   0,  23,
              -3,  17,  30,  37,  37,  30,  17,  -3,
              42,  45,  23,  19,  19,  23,  45,  42,
             -25, -47,   5,   6,   6,   5, -47, -25,
             -41,  11, -24, -36, -36, -24,  11, -41,
        },
        // Bishop endgame bonuses
        {
             -57, -28,  -2,  -8,  -8,  -2, -28, -57,
             -33, -43, -14,   1,   1, -14, -43, -33,
             -14,  -6, -17,  10,  10, -17,  -6, -14,
             -16, -15,   4,   1,   1,   4, -15, -16,
              -7,   4, -18,  21,  21, -18,   4,  -7,
             -12,  29, -15,  -1,  -1, -15,  29, -12,
             -17, -23,   9,  11,  11,   9, -23, -17,
             -34,  -8, -39, -14, -14, -39,  -8, -34,
        }
    },
    {
        // Rook midgame bonuses
        {
             -25,  -2,   6,  12,  12,   6,  -2, -25,
             -51, -25,  -1, -33, -33,  -1, -25, -51,
             -46, -22, -31, -23, -23, -31, -22, -46,
             -39, -13, -24, -29, -29, -24, -13, -39,
               3,   3,  14,   8,   8,  14,   3,   3,
              -6,  23,  19,  50,  50,  19,  23,  -6,
              13, -17,  17,  42,  42,  17, -17,  13,
             -17, -12,  -6,  12,  12,  -6, -12, -17,
        },
        // Rook endgame bonuses
        {
              -7, -18, -13, -19, -19, -13, -18,  -7,
              -5, -20, -23,  -2,  -2, -23, -20,  -5,
               5,  -5, -10, -13, -13, -10,  -5,   5,
               9,  24,   4,  15,  15,   4,  24,   9,
              12,  19,   7,  17,  17,   7,  19,  12,
              11,  14,  10,   7,   7,  10,  14,  11,
               8,  26,  29,  18,  18,  29,  26,   8,
              11,   5,  26,  19,  19,  26,   5,  11,
        }
    },
    {
        // Queen midgame bonuses
        {
               6,   9, -14,   3,   3, -14,   9,   6,
              17,  18,  12,  14,  14,  12,  18,  17,
              12,   0,   5, -13, -13,   5,   0,  12,
              14,   3, -21, -16, -16, -21,   3,  14,
              30,  -8,  -3, -34, -34,  -3,  -8,  30,
              62,  43,   6,  17,  17,   6,  43,  62,
               2, -76,   6,   5,   5,   6, -76,   2,
             -33,  -1,  -4,   2,   2,  -4,  -1, -33,
        },
        // Queen endgame bonuses
        {
             -63, -74, -82, -21, -21, -82, -74, -63,
             -55, -55, -56, -39, -39, -56, -55, -55,
             -21, -20, -14,   5,   5, -14, -20, -21,
              -5,  15,  13,  25,  25,  13,  15,  -5,
             -21,  31,  17,  38,  38,  17,  31, -21,
             -14,   2,  20,  24,  24,  20,   2, -14,
             -18,   9,  -6,  24,  24,  -6,   9, -18,
             -53, -39, -36, -29, -29, -36, -39, -53,
        }
    },
    {
        // King midgame bonuses
        {
             239, 264, 224, 159, 159, 224, 264, 239,
             266, 267, 233, 223, 223, 233, 267, 266,
             170, 230, 175, 146, 146, 175, 230, 170,
             141, 160, 120, 104, 104, 120, 160, 141,
             136, 154,  97,  69,  69,  97, 154, 136,
             107, 125,  81,  35,  35,  81, 125, 107,
              71, 110,  61,  34,  34,  61, 110,  71,
              47,  72,  36,  -1,  -1,  36,  72,  47,
        },
        // King endgame bonuses
        {
             -28,  36,  78, 102, 102,  78,  36, -28,
              53,  84, 136, 159, 159, 136,  84,  53,
              99, 126, 171, 207, 207, 171, 126,  99,
             114, 154, 204, 217, 217, 204, 154, 114,
             120, 194, 217, 247, 247, 217, 194, 120,
             114, 196, 235, 207, 207, 235, 196, 114,
              38, 155, 143, 142, 142, 143, 155,  38,
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

inline constexpr TaperedScore isolated_pawn = {-22, -20};
inline constexpr TaperedScore backward_pawn = {-20, -18};
inline constexpr TaperedScore doubled_pawn  = {-2, -35};
inline constexpr TaperedScore passed_pawn[] = {
    {0, 0}, {5, 0}, {2, 12}, {-7, 59}, {31, 120}, {98, 203}, {196, 281}, {0, 0}};
inline constexpr TaperedScore reachable_outpost       = {26, 20};
inline constexpr TaperedScore bishop_outpost          = {57, -17};
inline constexpr TaperedScore knight_outpost          = {68, 26};
inline constexpr TaperedScore minor_pawn_shield       = {18, 2};
inline constexpr TaperedScore bishop_long_diagonal    = {38, 27};
inline constexpr TaperedScore bishop_pair             = {44, 106};
inline constexpr TaperedScore bishop_blockers         = {-1, -7};
inline constexpr TaperedScore rook_closed_file        = {-5, -34};
inline constexpr TaperedScore king_zone_xray_attack   = {20, 15};
inline constexpr TaperedScore queen_discovered_attack = {-51, 55};

// bonus for rook on open files: [0 = semi-open, 1 = fully open]
inline constexpr TaperedScore rook_open_file[] = {{16, 7}, {54, -6}};

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
    TaperedScore::Zero, TaperedScore::Zero, {-74, -60}, {-71, -55}, {-44, -68}, {-99, -12}};

// Piece mobility scores (index = # of legal moves)
inline constexpr TaperedScore knight_mob[] = {
    {-70, -88},
    {-75, -89},
    {-42, -21},
    {-25, -20},
    {-8, -5},
    {-2, 2},
    {12, 12},
    {31, 4},
    {33, -1},
};

inline constexpr TaperedScore bishop_mob[] = {
    {-88, -32},
    {-51, -71},
    {-29, -1},
    {-6, -7},
    {8, 7},
    {28, 23},
    {34, 31},
    {40, 36},
    {50, 23},
    {45, 35},
    {66, 23},
    {85, 21},
    {79, 35},
    {57, 33},
};

inline constexpr TaperedScore rook_mob[] = {
    {-155, -98},
    {-83, -14},
    {-53, 22},
    {-58, 65},
    {-56, 73},
    {-50, 87},
    {-41, 100},
    {-40, 106},
    {-29, 111},
    {-21, 120},
    {-9, 124},
    {32, 104},
    {33, 107},
    {17, 103},
    {48, 90},
};

inline constexpr TaperedScore queen_mob[] = {
    {-20, -32}, {-12, -14}, {-15, -1}, {-21, 5},  {-26, 12}, {-7, 12},  {9, 17},
    {17, 8},    {25, 38},   {31, 59},  {42, 48},  {44, 64},  {47, 79},  {58, 78},
    {46, 92},   {55, 113},  {68, 98},  {59, 105}, {65, 92},  {60, 104}, {66, 105},
    {80, 108},  {74, 113},  {68, 104}, {72, 116}, {72, 120}, {76, 124}, {80, 140},
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
