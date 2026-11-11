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

inline constexpr int tempo_bonus = 20;
inline constexpr int scale_limit = 64;
inline constexpr int scale_base = 48;
inline constexpr int scale_per_pawn = 4;
inline constexpr int phase_limit = 128;

// EG-only interpolation from the pawn-count base to material-aware conversion caps.
inline constexpr TaperedScore endgame_scale_strength = {0, 64};

inline constexpr TaperedScore pawn = {100, 202};
inline constexpr TaperedScore knight = {602, 679};
inline constexpr TaperedScore bishop = {613, 709};
inline constexpr TaperedScore rook = {880, 1090};
inline constexpr TaperedScore queen = {2075, 2078};

inline constexpr int material_mg = 4 * knight.mg + 4 * bishop.mg + 4 * rook.mg + 2 * queen.mg;
inline constexpr int material_eg = 0;

namespace masks {

inline constexpr Bitboard dark_squares = 0xAA55AA55AA55AA55ull;
inline constexpr Bitboard light_squares = 0x55AA55AA55AA55AAull;
inline constexpr Bitboard center_files =
    (bb::file(FILE3) | bb::file(FILE4) | bb::file(FILE5) | bb::file(FILE6)) & ~bb::rank(RANK8);
inline constexpr Bitboard center_squares = bb::set(D4, E4, D5, E5);
inline constexpr Bitboard w_outposts = bb::rank(RANK4) | bb::rank(RANK5) | bb::rank(RANK6);
inline constexpr Bitboard b_outposts = bb::rank(RANK3) | bb::rank(RANK4) | bb::rank(RANK5);

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
               5,  25,   8,  19,  25,  10,  24,   0,
               6,  12,   8,  37,  41,   6,   5,   3,
               4,  17,  41,  79,  82,  30,  10,   7,
              13,  21,  54,  83,  92,  52,  16,  18,
              34,  18,  63,  73,  65,  69, -10,  -1,
              24,  -3,  22,  25,  12,  25,   7,  25,
               0,   0,   0,   0,   0,   0,   0,   0,
        },
        // Pawn endgame bonuses
        {
               0,   0,   0,   0,   0,   0,   0,   0,
               0, -17,   8,  -7, -14,   0, -13,   9,
               5,  -4,   3, -11, -18,  -7,  -3,  11,
              20,  20, -10, -25, -33, -15,  16,  21,
              46,  30, -12, -27, -34,  -6,  32,  46,
              71,  89,  24, -48, -41,   5,  84,  78,
              64,  15,  19, -20,   2,  42,  41,  61,
               0,   0,   0,   0,   0,   0,   0,   0,
        }
    },
    {
        // Knight midgame bonuses
        {
            -139, -72, -39,  -4,  -4, -39, -72,-139,
             -21, -19, -18, -13, -13, -18, -19, -21,
             -44, -12,   5,  25,  25,   5, -12, -44,
               2,  27,  29,  28,  28,  29,  27,   2,
              24,  25,  53,  55,  55,  53,  25,  24,
              -7,  44,  51,  98,  98,  51,  44,  -7,
             -21, -43,  63,  48,  48,  63, -43, -21,
            -218, -66, -62, -24, -24, -62, -66,-218,
        },
        // Knight endgame bonuses
        {
             -69, -30, -25, -25, -25, -25, -30, -69,
             -49, -31, -28,  -6,  -6, -28, -31, -49,
              -9,  -9,  -6,  11,  11,  -6,  -9,  -9,
             -21, -14,  10,  10,  10,  10, -14, -21,
             -16,  -2,  -5,   3,   3,  -5,  -2, -16,
             -36, -29, -16, -15, -15, -16, -29, -36,
             -23, -15, -30, -11, -11, -30, -15, -23,
            -106, -55, -42,  -9,  -9, -42, -55,-106,
        }
    },
    {
        // Bishop midgame bonuses
        {
             -24,  34,  -8,   2,   2,  -8,  34, -24,
              33,   9,  37,   3,   3,  37,   9,  33,
              24,  44,   4,  21,  21,   4,  44,  24,
              24,   8,  12,  41,  41,  12,   8,  24,
              -5,  19,  33,  39,  39,  33,  19,  -5,
              44,  44,  21,  28,  28,  21,  44,  44,
             -22, -51,   2,   5,   5,   2, -51, -22,
             -41,  11, -24, -37, -37, -24,  11, -41,
        },
        // Bishop endgame bonuses
        {
             -56, -27,  -5,  -8,  -8,  -5, -27, -56,
             -29, -45, -21,   1,   1, -21, -45, -29,
             -11,  -8, -22,  11,  11, -22,  -8, -11,
             -18, -10,   5,   6,   6,   5, -10, -18,
              -9,   5, -13,  14,  14, -13,   5,  -9,
             -15,  26, -19,   2,   2, -19,  26, -15,
             -12, -21,  11,   6,   6,  11, -21, -12,
             -33,  -4, -36, -13, -13, -36,  -4, -33,
        }
    },
    {
        // Rook midgame bonuses
        {
             -25, -10,  -3,   5,   5,  -3, -10, -25,
             -55, -27, -10, -28, -28, -10, -27, -55,
             -49, -21, -35, -29, -29, -35, -21, -49,
             -36, -18, -24, -30, -30, -24, -18, -36,
               8,   7,  17,   9,   9,  17,   7,   8,
              -2,  24,  17,  52,  52,  17,  24,  -2,
              16, -23,  22,  48,  48,  22, -23,  16,
             -14, -10,  -6,   8,   8,  -6, -10, -14,
        },
        // Rook endgame bonuses
        {
              -7, -21, -17, -27, -27, -17, -21,  -7,
              -4, -22, -23,  -6,  -6, -23, -22,  -4,
               5,  -6,  -8, -18, -18,  -8,  -6,   5,
              15,  22,  10,  12,  12,  10,  22,  15,
              19,  25,  13,  17,  17,  13,  25,  19,
              15,  12,  15,   7,   7,  15,  12,  15,
              10,  29,  31,  24,  24,  31,  29,  10,
              15,   7,  27,  12,  12,  27,   7,  15,
        }
    },
    {
        // Queen midgame bonuses
        {
               5,  10,  -7,   3,   3,  -7,  10,   5,
               9,  11,  13,   9,   9,  13,  11,   9,
              11,   4,   8, -17, -17,   8,   4,  11,
              13,   7, -16, -20, -20, -16,   7,  13,
              38,  -8,  -5, -36, -36,  -5,  -8,  38,
              59,  42,   2,  15,  15,   2,  42,  59,
               3, -85,   1,   4,   4,   1, -85,   3,
             -33,   2,  -3,   3,   3,  -3,   2, -33,
        },
        // Queen endgame bonuses
        {
             -64, -75, -82, -21, -21, -82, -75, -64,
             -57, -61, -59, -42, -42, -59, -61, -57,
             -24, -20, -15,   2,   2, -15, -20, -24,
              -5,  15,  16,  26,  26,  16,  15,  -5,
             -19,  34,  20,  43,  43,  20,  34, -19,
             -16,   3,  23,  25,  25,  23,   3, -16,
             -16,  12,  -6,  26,  26,  -6,  12, -16,
             -51, -37, -34, -25, -25, -34, -37, -51,
        }
    },
    {
        // King midgame bonuses
        {
             236, 264, 226, 153, 153, 226, 264, 236,
             258, 259, 230, 226, 226, 230, 259, 258,
             171, 227, 179, 152, 152, 179, 227, 171,
             141, 162, 122, 108, 108, 122, 162, 141,
             138, 154,  99,  70,  70,  99, 154, 138,
             108, 127,  82,  37,  37,  82, 127, 108,
              71, 111,  63,  35,  35,  63, 111,  71,
              47,  72,  36,  -1,  -1,  36,  72,  47,
        },
        // King endgame bonuses
        {
             -29,  36,  80, 104, 104,  80,  36, -29,
              47,  83, 132, 155, 155, 132,  83,  47,
             101, 129, 179, 211, 211, 179, 129, 101,
             120, 160, 210, 224, 224, 210, 160, 120,
             127, 196, 227, 252, 252, 227, 196, 127,
             120, 204, 239, 214, 214, 239, 204, 120,
              39, 158, 149, 147, 147, 149, 158,  39,
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
    Square relative = square::relative(sq, c);
    TaperedScore square_scores = {
        .mg = piece_squares[piece_slot(pt)][std::to_underlying(Phase::Midgame)][relative],
        .eg = piece_squares[piece_slot(pt)][std::to_underlying(Phase::Endgame)][relative]};

    return (square_scores * c * 2) - square_scores;
}

inline constexpr TaperedScore isolated_pawn = {-3, -17};
inline constexpr TaperedScore backward_pawn = {-17, -14};
inline constexpr TaperedScore doubled_pawn = {2, -34};
inline constexpr TaperedScore connected_pawn_link = {32, 12};
// Additional penalties for weak pawns on files without opposing pawns.
inline constexpr TaperedScore isolated_open_pawn = {-14, -14};
inline constexpr TaperedScore backward_open_pawn = {-13, -6};
inline constexpr TaperedScore passed_pawn[] =
    {{0, 0}, {1, 1}, {-8, 9}, {-2, 65}, {34, 122}, {102, 211}, {198, 296}, {0, 0}};
// Provisional passer weights, indexed by relative rank. These have not been tuned.
inline constexpr TaperedScore candidate_pawn[8] =
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {8, 30}, {25, 52}, {0, 0}, {0, 0}};
inline constexpr TaperedScore passed_blocked[8] =
    {{0, 0}, {0, 0}, {0, 0}, {0, -16}, {0, -30}, {0, -52}, {0, -74}, {0, 0}};
inline constexpr TaperedScore passed_safe_advance[8] =
    {{0, 0}, {0, 0}, {0, 0}, {0, 8}, {0, 15}, {0, 26}, {0, 37}, {0, 0}};
inline constexpr TaperedScore passed_own_king_distance[8] =
    {{0, 0}, {0, 0}, {0, 0}, {0, -2}, {0, -4}, {0, -6}, {0, -8}, {0, 0}};
inline constexpr TaperedScore passed_enemy_king_distance[8] =
    {{0, 0}, {0, 0}, {0, 0}, {0, 2}, {0, 4}, {0, 6}, {0, 8}, {0, 0}};
inline constexpr TaperedScore reachable_outpost = {21, 26};
inline constexpr TaperedScore bishop_outpost = {51, -14};
inline constexpr TaperedScore knight_outpost = {60, 25};
inline constexpr TaperedScore minor_pawn_shield = {13, 13};
inline constexpr TaperedScore bishop_long_diagonal = {35, 31};
inline constexpr TaperedScore bishop_pair = {42, 108};
inline constexpr TaperedScore bishop_blockers = {-1, -8};
inline constexpr TaperedScore rook_closed_file = {-6, -31};
inline constexpr TaperedScore king_zone_xray_attack = {15, 12};
inline constexpr TaperedScore queen_discovered_attack = {-53, 52};

// bonus for rook on open files: [0 = semi-open, 1 = fully open]
inline constexpr TaperedScore rook_open_file[] = {{29, 1}, {62, -6}};

// shelter bonus for friendly pawn rank [index = pawn rank, 0 = no pawn]
inline constexpr TaperedScore pawn_shelter[] =
    {{-30, 0}, {60, 0}, {35, 0}, {-20, 0}, {-5, 0}, {-20, 0}, {-80, 0}};

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
inline constexpr TaperedScore king_file[] =
    {{20, 0}, {5, 0}, {-15, 0}, {-30, 0}, {-30, 0}, {-15, 0}, {5, 0}, {20, 0}};

// Penalty for potentially hanging piece [index = piece type]
inline constexpr TaperedScore weak_piece[] =
    {TaperedScore::Zero, TaperedScore::Zero, {-68, -56}, {-66, -51}, {-38, -68}, {-90, -4}};

// Bonus for a new piece target after an eligible pawn push [N, B, R, Q].
inline constexpr TaperedScore pawn_push_threat[4] = {
    {19, 14},
    {18, 18},
    {26, 24},
    {29, 0},
};

// Piece mobility scores (index = # of legal moves)
inline constexpr TaperedScore knight_mob[] = {
    {-76, -83},
    {-74, -86},
    {-42, -30},
    {-21, -20},
    {-15, -5},
    {-3, 0},
    {12, 12},
    {27, 14},
    {30, 0},
};

inline constexpr TaperedScore bishop_mob[] = {
    {-99, -31},
    {-44, -71},
    {-30, -10},
    {-7, -10},
    {7, 11},
    {25, 17},
    {35, 23},
    {40, 36},
    {51, 32},
    {48, 35},
    {63, 23},
    {89, 18},
    {78, 31},
    {57, 28},
};

inline constexpr TaperedScore rook_mob[] = {
    {-157, -99},
    {-81, -11},
    {-52, 32},
    {-50, 65},
    {-51, 78},
    {-46, 88},
    {-38, 103},
    {-36, 113},
    {-25, 119},
    {-18, 125},
    {-9, 125},
    {32, 104},
    {21, 107},
    {19, 100},
    {48, 97},
};

inline constexpr TaperedScore queen_mob[] = {
    {-20, -32}, {-13, -14}, {-15, -1}, {-29, 3},  {-22, 12}, {-7, 11},  {4, 18},
    {21, 9},    {24, 36},   {31, 56},  {39, 54},  {44, 64},  {46, 78},  {56, 77},
    {48, 90},   {54, 106},  {58, 98},  {56, 106}, {67, 97},  {58, 103}, {67, 107},
    {79, 106},  {75, 113},  {67, 102}, {71, 115}, {72, 120}, {76, 124}, {80, 140},
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
inline constexpr int safe_check_danger[N_PIECETYPES] = {0, 0, 320, 240, 360, 280};
inline constexpr int unsafe_check_danger[N_PIECETYPES] = {0, 0, 35, 30, 25, 5};

inline constexpr int pinned_piece_danger = 30;
inline constexpr int weak_king_zone_danger = 80;

} // namespace eval
