#pragma once

#include "defs.hpp"
#include "score.hpp"
#include "util.hpp"

namespace eval {

constexpr int tempo_bonus = 20;
constexpr int scale_limit = 64;
constexpr int phase_limit = 128;
constexpr int material_mg = 10000;
constexpr int material_eg = 2500;

constexpr Score pawn   = {PAWN_MG, PAWN_EG};
constexpr Score knight = {KNIGHT_MG, KNIGHT_EG};
constexpr Score bishop = {BISHOP_MG, BISHOP_EG};
constexpr Score rook   = {ROOK_MG, ROOK_EG};
constexpr Score queen  = {QUEEN_MG, QUEEN_EG};

constexpr Score pieces[N_PIECES - 1][N_COLORS] = {
    {-pawn, pawn},
    {-knight, knight},
    {-bishop, bishop},
    {-rook, rook},
    {-queen, queen},
    {Score::Zero, Score::Zero},
};

constexpr int piece_squares[6][2][64] = {
    // clang-format off
    {
        // Pawn midgame bonuses
        {
             0,   0,   0,   0,   0,   0,   0,   0,
             2,   2,   8,  14,  14,  15,   6,  -4,
            -7, -12,   9,  22,  22,  18,   4, -18,
            -3, -19,   5,  30,  30,  14,   3,  -6,
            10,   0, -10,   1,   9,  -2, -10,   4,
             4, -10,  -6,  18,  -6,  -4, -12,  -6,
            -6,   6,  -2, -10,   4, -13,   8,  -6,
             0,   0,   0,   0,   0,   0,   0,   0,
        },
        // Pawn endgame bonuses
        {
             0,   0,   0,   0,   0,   0,   0,   0,
            -8,  -5,   8,   0,  11,   6,  -4, -15,
            -8,  -8,  -8,   3,   3,   2,  -5,  -3,
             5,  -2,  -6,  -3, -10, -10,  -8,  -7,
             8,   4,   3,  -4,  -4,  -4,  11,   7,
            23,  16,  17,  23,  24,   6,   5,  10,
             0,  -9,  10,  17,  20,  15,   3,   6,
             0,   0,   0,   0,   0,   0,   0,   0,
        }
    }, {
        // Knight midgame bonuses
        {
            -141,  -74,  -60,  -59,  -59,  -60,  -74, -141,
             -62,  -33,  -22,  -12,  -12,  -22,  -33,  -62,
             -49,  -14,    5,   10,   10,    5,  -14,  -49,
             -28,    6,   32,   39,   39,   32,    6,  -28,
             -27,   10,   35,   41,   41,   35,   10,  -27,
              -7,   18,   47,   43,   43,   47,   18,   -7,
             -54,  -22,    3,   30,   30,    3,  -22,  -54,
            -162,  -67,  -45,  -21,  -21,  -45,  -67, -162,
        },
        // Knight endgame bonuses
        {
            -77,  -52,  -40,  -17,  -17,  -40,  -52,  -77,
            -54,  -44,  -15,    6,    6,  -15,  -44,  -54,
            -32,  -22,   -6,   23,   23,   -6,  -22,  -32,
            -28,   -2,   10,   23,   23,   10,   -2,  -28,
            -36,  -13,    7,   31,   31,    7,  -13,  -36,
            -41,  -35,  -13,   14,   14,  -13,  -35,  -41,
            -55,  -40,  -41,   10,   10,  -41,  -40,  -55,
            -81,  -71,  -45,  -14,  -14,  -45,  -71,  -81,
        }
    }, {
        // Bishop midgame bonuses
        {
            -43,   -4,   -6,  -19,  -19,   -6,   -4,  -43,
            -12,    6,   15,    3,    3,   15,    6,  -12,
             -6,   17,   -4,   14,   14,   -4,   17,   -6,
             -4,    9,   20,   31,   31,   20,    9,   -4,
            -10,   23,   18,   25,   25,   18,   23,  -10,
            -13,    5,    1,    9,    9,    1,    5,  -13,
            -14,  -11,    4,    0,    0,    4,  -11,  -14,
            -39,    1,  -11,  -19,  -19,  -11,    1,  -39,
        },
        // Bishop endgame bonuses
        {
            -46,  -24,  -30,  -10,  -10,  -30,  -24,  -46,
            -30,  -10,  -14,    1,    1,  -14,  -10,  -30,
            -13,   -1,   -2,    8,    8,   -2,   -1,  -13,
            -16,   -5,    0,   14,   14,    0,   -5,  -16,
            -14,   -1,  -11,   12,   12,  -11,   -1,  -14,
            -24,    5,    3,    5,    5,    3,    5,  -24,
            -25,  -16,   -1,    1,    1,   -1,  -16,  -25,
            -37,  -34,  -30,  -19,  -19,  -30,  -34,  -37,
        }
    }, {
        // Rook midgame bonuses
        {
            -25,  -16,  -11,   -4,   -4,  -11,  -16,  -25,
            -17,  -10,   -6,    5,    5,   -6,  -10,  -17,
            -20,   -9,   -1,    2,    2,   -1,   -9,  -20,
            -10,   -4,   -3,   -5,   -5,   -3,   -4,  -10,
            -22,  -12,   -3,    2,    2,   -3,  -12,  -22,
            -18,   -2,    5,   10,   10,    5,   -2,  -18,
             -2,   10,   13,   15,   15,   13,   10,   -2,
            -14,  -15,   -1,    7,    7,   -1,  -15,  -14,
        },
        // Rook endgame bonuses
        {
             -7,  -10,   -8,   -7,   -7,   -8,  -10,   -7,
            -10,   -7,   -1,   -2,   -2,   -1,   -7,  -10,
              5,   -6,   -2,   -5,   -5,   -2,   -6,    5,
             -5,    1,   -7,    6,    6,   -7,    1,   -5,
             -4,    6,    6,   -5,   -5,    6,    6,   -4,
              5,    1,   -6,    8,    8,   -6,    1,    5,
              3,    4,   16,   -4,   -4,   16,    4,    3,
             15,    0,   15,   10,   10,   15,    0,   15,
        }
    }, {
        // Queen midgame bonuses
        {
             2,   -4,   -4,    3,    3,   -4,   -4,    2,
            -2,    4,    6,   10,   10,    6,    4,   -2,
            -2,    5,   10,    6,    6,   10,    5,   -2,
             3,    4,    7,    6,    6,    7,    4,    3,
             0,   11,   10,    4,    4,   10,   11,    0,
            -3,    8,    5,    6,    6,    5,    8,   -3,
            -4,    5,    8,    6,    6,    8,    5,   -4,
            -2,   -2,    1,   -2,   -2,    1,   -2,   -2,
        },
        // Queen endgame bonuses
        {
            -56,  -46,  -38,  -21,  -21,  -38,  -46,  -56,
            -44,  -25,  -18,   -3,   -3,  -18,  -25,  -44,
            -31,  -15,   -7,    2,    2,   -7,  -15,  -31,
            -19,   -2,   10,   19,   19,   10,   -2,  -19,
            -23,   -5,    7,   17,   17,    7,   -5,  -23,
            -31,  -15,  -10,    1,    1,  -10,  -15,  -31,
            -40,  -22,  -19,   -6,   -6,  -19,  -22,  -40,
            -60,  -42,  -35,  -29,  -29,  -35,  -42,  -60,
        }
    }, {
        // King midgame bonuses
        {
            219, 264, 219, 160, 160, 219, 264, 219,
            224, 244, 189, 144, 144, 189, 244, 224,
            157, 208, 136,  97,  97, 136, 208, 157,
            132, 153, 111,  79,  79, 111, 153, 132,
            124, 144,  85,  56,  56,  85, 144, 124,
             99, 117,  65,  25,  25,  65, 117,  99,
             71,  97,  52,  27,  27,  52,  97,  71,
             47,  72,  36,  -1,  -1,  36,  72,  47,
        },
        // King endgame bonuses
        {
             1,  36,  69,  61,  61,  69,  36,   1,
            43,  81, 107, 109, 109, 107,  81,  43,
            71, 105, 136, 141, 141, 136, 105,  71,
            83, 126, 138, 138, 138, 138, 126,  83,
            77, 133, 160, 160, 160, 160, 133,  77,
            74, 139, 148, 153, 153, 148, 139,  74,
            38,  98,  93, 106, 106,  93,  98,  38,
             9,  47,  59,  63,  63,  59,  47,   9,
        } 
    }
    // clang-format on
};

constexpr Score piece(PieceType pt, Color c = WHITE) {
    return pieces[pt - 1][c];
}

constexpr Score piece_sq(PieceType pt, Color c, Square sq) {
    Square relative = relative_square(sq, c);
    Score  score    = {.mg = piece_squares[pt - 1][MIDGAME][relative],
                       .eg = piece_squares[pt - 1][ENDGAME][relative]};

    return (score * c * 2) - score;
}

constexpr Score iso_pawn           = {-5, -15};
constexpr Score backward_pawn      = {-10, -25};
constexpr Score doubled_pawn       = {-10, -50};
constexpr Score reachable_outpost  = {30, 20};
constexpr Score bishop_outpost     = {30, 20};
constexpr Score knight_outpost     = {50, 30};
constexpr Score minor_pawn_shield  = {20, 5};
constexpr Score bishop_long_diag   = {40, 0};
constexpr Score bishop_pair        = {50, 80};
constexpr Score bishop_blockers    = {-2, -6};
constexpr Score rook_closed_file   = {-10, -5};
constexpr Score kingzone_xray_att  = {20, 0};
constexpr Score queen_discover_att = {-50, -25};

// bonus for rook on open files: [0 = semi-open, 1 = fully open]
constexpr Score rook_open_file[] = {{20, 10}, {40, 20}};

// shelter bonus for friendly pawn rank [index = pawn rank, 0 = no pawn]
constexpr Score pawn_shelter[] = {
    {-30, 0}, {60, 0}, {35, 0}, {-20, 0}, {-5, 0}, {-20, 0}, {-80, 0}};

// Pawn storm penalty by pawn rank:
// [0 = unblocked, 1 = blocked][index = pawn rank, 0 = no pawn)]
constexpr Score pawn_storm[][7] = {
    {{0, 0}, {-20, 0}, {-120, 0}, {-60, 0}, {-45, 0}, {-20, 0}, {-10, 0}},
    {{0, 0}, {0, 0}, {-60, -60}, {0, -20}, {5, -15}, {10, -10}, {15, -5}}};

// score for king on open/closed files: [friendly file][enemy file] (0 = closed, 1 = open)
constexpr Score king_open_file[][2] = {
    {{20, -10}, {10, 5}},
    {{0, 0}, {-10, 5}},
};

// Score for king based on file [index = king file]
constexpr Score king_file[] = {
    {20, 0}, {5, 0}, {-15, 0}, {-30, 0}, {-30, 0}, {-15, 0}, {5, 0}, {20, 0}};

// Score for potentially hanging piece [index = piece type]
constexpr Score weak_piece[] = {
    Score::Zero, Score::Zero, {-20, -10}, {-25, -15}, {-50, -25}, {-100, -50}};

// Piece mobility scores (index = # of legal moves)
constexpr Score knight_mob[] = {
    {-40, -48},
    {-32, -36},
    {-8, -20},
    {-2, -12},
    {2, 6},
    {8, 8},
    {12, 12},
    {16, 16},
    {24, 16},
};

constexpr Score bishop_mob[] = {
    {-32, -40},
    {-16, -16},
    {8, -4},
    {16, 8},
    {24, 16},
    {32, 24},
    {32, 36},
    {40, 36},
    {40, 40},
    {44, 48},
    {48, 48},
    {56, 56},
    {56, 56},
    {64, 64},
};

constexpr Score rook_mob[] = {
    {-40, -56},
    {-16, -8},
    {0, 12},
    {0, 28},
    {4, 44},
    {8, 64},
    {12, 64},
    {20, 80},
    {28, 88},
    {28, 88},
    {28, 96},
    {32, 104},
    {36, 108},
    {40, 112},
    {44, 120},
};

constexpr Score queen_mob[] = {
    {-20, -32}, {-12, -20}, {-4, -4},  {-4, 12},  {12, 24},  {16, 36},  {16, 40},
    {24, 48},   {28, 48},   {36, 60},  {40, 60},  {44, 64},  {44, 80},  {48, 80},
    {48, 88},   {48, 88},   {48, 88},  {48, 92},  {52, 96},  {56, 96},  {60, 100},
    {68, 108},  {68, 112},  {68, 112}, {72, 116}, {72, 120}, {76, 124}, {80, 140},
};

constexpr const Score* mobility[] = {
    nullptr,
    nullptr,
    knight_mob,
    bishop_mob,
    rook_mob,
    queen_mob,
};

// danger values [index = piece type]
constexpr int kingzone_att_danger[N_PIECES] = {0, 0, 30, 22, 18, 5};
constexpr int safe_check_danger[N_PIECES]   = {0, 0, 320, 240, 360, 280};
constexpr int unsafe_check_danger[N_PIECES] = {0, 0, 35, 30, 25, 5};

constexpr int pinned_piece_danger  = 30;
constexpr int weak_kingzone_danger = 80;

}; // namespace eval
