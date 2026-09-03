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

inline constexpr TaperedScore pawn   = {100, 203};
inline constexpr TaperedScore knight = {580, 657};
inline constexpr TaperedScore bishop = {593, 693};
inline constexpr TaperedScore rook   = {901, 1053};
inline constexpr TaperedScore queen  = {2028, 2080};

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
            15,  26,   8,  33,  22,  20,  27,   3,
            25,  20,  22,  62,  55,  35,  25,  13,
             6,  10,  33,  88,  88,  50,  16,  -3,
            13,   1,  37,  86,  94,  42,  18,   3,
             4,  42,  52,  42,  45,  75,  -7, -28,
            23,  -9,  28,  17,   3,  35,   7,  15,
             0,   0,   0,   0,   0,   0,   0,   0,
        },
        // Pawn endgame bonuses
        {
             0,   0,   0,   0,   0,   0,   0,   0,
            -11,  -6,   8,  -4, -12, -12,  -6,  -1,
            -10,   0,   9, -10, -11,  -5,   5,   2,
             4,  23,  -8, -29, -31, -13,  10,  13,
            31,  24,   1, -30, -49,  -4,  31,  35,
            68,  86,  13,  -6, -32,   8,  99,  56,
            60,  -2,  24, -18,  11,  50,  19,  43,
             0,   0,   0,   0,   0,   0,   0,   0,
        }
    }, {
        // Knight midgame bonuses
        {
            -148, -58, -60, -11, -11, -60, -58,-148,
            -22, -39, -26, -15, -15, -26, -39, -22,
            -20,  12,   5,  11,  11,   5,  12, -20,
             0,  18,  23,  22,  22,  23,  18,   0,
            27,  24,  51,  48,  48,  51,  24,  27,
            -28,  51,  54, 101, 101,  54,  51, -28,
            -41, -44,  44,  38,  38,  44, -44, -41,
            -202, -66, -63, -22, -22, -63, -66,-202,
        },
        // Knight endgame bonuses
        {
            -78, -15, -22, -31, -31, -22, -15, -78,
            -50, -46, -16,  -6,  -6, -16, -46, -50,
            -4, -11,  -6,  15,  15,  -6, -11,  -4,
            -17, -15,  -2,  14,  14,  -2, -15, -17,
            -21,   6,   5,   9,   9,   5,   6, -21,
            -42, -27, -17,   0,   0, -17, -27, -42,
            -33, -30, -31,  -8,  -8, -31, -30, -33,
            -106, -55, -49,  -4,  -4, -49, -55,-106,
        }
    }, {
        // Bishop midgame bonuses
        {
            -40,  12,  -9,  -4,  -4,  -9,  12, -40,
            29,   3,  17,   3,   3,  17,   3,  29,
            19,  48,   7,   9,   9,   7,  48,  19,
            10,  -3,   7,  12,  12,   7,  -3,  10,
            -18,   3,  36,   6,   6,  36,   3, -18,
            60,  47,  22,  12,  12,  22,  47,  60,
            -29, -40,   3,  -2,  -2,   3, -40, -29,
            -46,  13, -20, -38, -38, -20,  13, -46,
        },
        // Bishop endgame bonuses
        {
            -60, -20,  14, -18, -18,  14, -20, -60,
            -28, -37, -19,   1,   1, -19, -37, -28,
            -17, -10, -21,   9,   9, -21, -10, -17,
            -23, -10,   4,  -7,  -7,   4, -10, -23,
            -15,  14, -34,  14,  14, -34,  14, -15,
            -10,  31,  -8,   5,   5,  -8,  31, -10,
            -16, -26,  13,  11,  11,  13, -26, -16,
            -39,  -5, -39, -25, -25, -39,  -5, -39,
        }
    }, {
        // Rook midgame bonuses
        {
            -25,  -8,  -2,   5,   5,  -2,  -8, -25,
            -59, -23,  -1, -34, -34,  -1, -23, -59,
            -52,  -5, -27, -19, -19, -27,  -5, -52,
            -47,   4, -19, -21, -21, -19,   4, -47,
            10,  10,  26,   4,   4,  26,  10,  10,
            -9,  22,  25,  35,  35,  25,  22,  -9,
            12, -15,  14,  41,  41,  14, -15,  12,
            -23, -21,  -9,   5,   5,  -9, -21, -23,
        },
        // Rook endgame bonuses
        {
            -7, -15, -19, -23, -23, -19, -15,  -7,
            -20, -26, -24,  11,  11, -24, -26, -20,
             9,   3, -13, -11, -11, -13,   3,   9,
             5,  13,  -4,   5,   5,  -4,  13,   5,
             5,  13,  10,  19,  19,  10,  13,   5,
            17,  17,  15,   2,   2,  15,  17,  17,
             8,  27,  22,  22,  22,  22,  27,   8,
             5,  -5,  23,  11,  11,  23,  -5,   5,
        }
    }, {
        // Queen midgame bonuses
        {
             3,  13,   0,   3,   3,   0,  13,   3,
            15,   2,  11,   4,   4,  11,   2,  15,
             7,   9,   7, -17, -17,   7,   9,   7,
            18,  -8, -17,  -4,  -4, -17,  -8,  18,
            53,  -9,   3, -32, -32,   3,  -9,  53,
            78,  50,   5,  29,  29,   5,  50,  78,
            -1, -60,  -2,   8,   8,  -2, -60,  -1,
            -27,  -2,  -7,   0,   0,  -7,  -2, -27,
        },
        // Queen endgame bonuses
        {
            -65, -69, -76, -21, -21, -76, -69, -65,
            -53, -51, -56, -33, -33, -56, -51, -53,
            -22, -17, -19,   4,   4, -19, -17, -22,
            -3,  18,  12,  29,  29,  12,  18,  -3,
            -16,  22,  15,  26,  26,  15,  22, -16,
            -14,   1,  17,  27,  27,  17,   1, -14,
            -23,   2, -14,  21,  21, -14,   2, -23,
            -56, -42, -39, -29, -29, -39, -42, -56,
        }
    }, {
        // King midgame bonuses
        {
            242, 264, 242, 163, 163, 242, 264, 242,
            267, 265, 215, 214, 214, 215, 265, 267,
            165, 223, 169, 130, 130, 169, 223, 165,
            142, 158, 116,  94,  94, 116, 158, 142,
            137, 153,  95,  65,  65,  95, 153, 137,
            104, 122,  77,  32,  32,  77, 122, 104,
            71, 108,  57,  33,  33,  57, 108,  71,
            47,  72,  36,  -1,  -1,  36,  72,  47,
        },
        // King endgame bonuses
        {
            -27,  36,  71, 103, 103,  71,  36, -27,
            50,  79, 134, 147, 147, 134,  79,  50,
            97, 119, 163, 190, 190, 163, 119,  97,
            108, 148, 186, 195, 195, 186, 148, 108,
            115, 179, 209, 238, 238, 209, 179, 115,
            101, 177, 215, 197, 197, 215, 177, 101,
            38, 147, 126, 137, 137, 126, 147,  38,
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

inline constexpr TaperedScore isolated_pawn = {-26, -12};
inline constexpr TaperedScore backward_pawn = {-18, -10};
inline constexpr TaperedScore doubled_pawn  = {-7, -42};
inline constexpr TaperedScore passed_pawn[] = {
    {0, 0}, {4, -10}, {-15, -8}, {5, 50}, {32, 112}, {127, 180}, {180, 258}, {0, 0}};
inline constexpr TaperedScore reachable_outpost       = {20, 28};
inline constexpr TaperedScore bishop_outpost          = {43, -4};
inline constexpr TaperedScore knight_outpost          = {69, 12};
inline constexpr TaperedScore minor_pawn_shield       = {10, 1};
inline constexpr TaperedScore bishop_long_diagonal    = {39, 22};
inline constexpr TaperedScore bishop_pair             = {41, 121};
inline constexpr TaperedScore bishop_blockers         = {-1, -7};
inline constexpr TaperedScore rook_closed_file        = {2, -40};
inline constexpr TaperedScore king_zone_xray_attack   = {16, 14};
inline constexpr TaperedScore queen_discovered_attack = {-40, 37};

// bonus for rook on open files: [0 = semi-open, 1 = fully open]
inline constexpr TaperedScore rook_open_file[] = {{31, 3}, {69, -13}};

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
    TaperedScore::Zero, TaperedScore::Zero, {-65, -47}, {-70, -58}, {-32, -59}, {-117, -22}};

// Piece mobility scores (index = # of legal moves)
inline constexpr TaperedScore knight_mob[] = {
    {-72, -81},
    {-58, -79},
    {-19, -15},
    {-16, -9},
    {-3, -4},
    {2, 2},
    {12, 12},
    {29, 22},
    {40, 7},
};

inline constexpr TaperedScore bishop_mob[] = {
    {-119, -17},
    {-53, -64},
    {-32, -23},
    {-5, -9},
    {5, 10},
    {22, 26},
    {33, 31},
    {40, 36},
    {52, 31},
    {59, 27},
    {73, 46},
    {84, 22},
    {76, 46},
    {54, 38},
};

inline constexpr TaperedScore rook_mob[] = {
    {-140, -95},
    {-58, -8},
    {-39, 34},
    {-32, 48},
    {-47, 67},
    {-38, 80},
    {-37, 97},
    {-35, 109},
    {-25, 108},
    {-7, 112},
    {-11, 129},
    {32, 104},
    {51, 97},
    {15, 112},
    {55, 92},
};

inline constexpr TaperedScore queen_mob[] = {
    {-20, -32}, {-11, -17}, {-13, -2}, {-33, 3},  {-20, 18}, {5, 17},   {20, 33},
    {17, 0},    {25, 29},   {28, 65},  {36, 53},  {44, 64},  {47, 69},  {51, 72},
    {55, 88},   {51, 104},  {52, 111}, {70, 100}, {83, 92},  {54, 98},  {74, 110},
    {88, 113},  {72, 114},  {66, 105}, {72, 116}, {72, 120}, {76, 124}, {80, 140},
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
