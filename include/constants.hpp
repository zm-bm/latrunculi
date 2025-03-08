#pragma once

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "types.hpp"

// FENs
const auto STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
           POS2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
           POS3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
           POS4W = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
           POS4B = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
           POS5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
           EMPTYFEN = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";

// movegen constants
const int MAX_MOVES = 256;

// search constants
const int MAX_DEPTH = 64;
const int MATESCORE = 16384;

// move gen constants
const Square KingOrigin[N_COLORS] = {E8, E1};
const Square KingDestinationOO[N_COLORS] = {G8, G1};
const Square KingDestinationOOO[N_COLORS] = {C8, C1};
const Square RookOriginOO[N_COLORS] = {H8, H1};
const Square RookOriginOOO[N_COLORS] = {A8, A1};
const U64 CastlePathOO[N_COLORS] = {0x6000000000000000ull, 0x0000000000000060ull};
const U64 CastlePathOOO[N_COLORS] = {0x0E00000000000000ull, 0x000000000000000Eull};
const U64 KingCastlePathOO[N_COLORS] = {0x7000000000000000ull, 0x0000000000000070ull};
const U64 KingCastlePathOOO[N_COLORS] = {0x1C00000000000000ull, 0x000000000000001Cull};

// eval constants
const U64 LIGHT_SQUARES = 0x55AA55AA55AA55AA;
const U64 DARK_SQUARES = 0xAA55AA55AA55AA55;
const U64 WHITE_OUTPOSTS = 0x0000FFFFFF000000;
const U64 BLACK_OUTPOSTS = 0x000000FFFFFF0000;
const U64 CENTER_FILES = 0x3C3C3C3C3C3C3C3C;
const U64 CENTER_SQUARES = 0x0000001818000000;

const int PAWN_VALUE_MG = 100;
const int KNIGHT_VALUE_MG = 630;
const int BISHOP_VALUE_MG = 660;
const int ROOK_VALUE_MG = 1000;
const int QUEEN_VALUE_MG = 2000;
const int PAWN_VALUE_EG = 166;
const int KNIGHT_VALUE_EG = 680;
const int BISHOP_VALUE_EG = 740;
const int ROOK_VALUE_EG = 1100;
const int QUEEN_VALUE_EG = 2150;

const int SCALE_LIMIT = 64;
const int PHASE_LIMIT = 128;
const int MG_LIMIT =
    2 * KNIGHT_VALUE_MG + 2 * BISHOP_VALUE_MG + 4 * ROOK_VALUE_MG + 2 * QUEEN_VALUE_MG;
const int EG_LIMIT = KNIGHT_VALUE_MG + BISHOP_VALUE_MG + 2 * ROOK_VALUE_MG;
const int TEMPO_BONUS = 25;

// clang-format off
const int PIECE_VALUES[N_PHASES][N_COLORS][N_PIECES] = {{
    {0, -PAWN_VALUE_MG, -KNIGHT_VALUE_MG, -BISHOP_VALUE_MG, -ROOK_VALUE_MG, -QUEEN_VALUE_MG, 0},
    {0, PAWN_VALUE_MG, KNIGHT_VALUE_MG, BISHOP_VALUE_MG, ROOK_VALUE_MG, QUEEN_VALUE_MG, 0}       
}, {
    {0, -PAWN_VALUE_EG, -KNIGHT_VALUE_EG, -BISHOP_VALUE_EG, -ROOK_VALUE_EG, -QUEEN_VALUE_EG, 0},
    {0, PAWN_VALUE_EG, KNIGHT_VALUE_EG, BISHOP_VALUE_EG, ROOK_VALUE_EG, QUEEN_VALUE_EG, 0}
}};

const std::array<int, N_SQUARES> PAWN_BONUS_MG = {
    0,   0,   0,   0,   0,   0,   0,   0,
    2,   2,   8,  15,  13,  15,   6,  -4,
   -7, -12,   9,  12,  26,  18,   4, -18,
   -3, -19,   5,  16,  32,  14,   3,  -6,
   10,   0, -10,   1,   9,  -2, -10,   4,
    4, -10,  -6,  18,  -6,  -4, -12,  -6,
   -6,   6,  -2, -10,   4, -13,   8,  -6,
    0,   0,   0,   0,   0,   0,   0,   0
};

const std::array<int, N_SQUARES> PAWN_BONUS_EG = {{
    0,   0,   0,   0,   0,   0,   0,   0,
   -8,  -5,   8,   0,  11,   6,  -4, -15,
   -8,  -8,  -8,   3,   3,   2,  -5,  -3,
    5,  -2,  -6,  -3, -10, -10,  -8,  -7,
    8,   4,   3,  -4,  -4,  -4,  11,   7,
   23,  16,  17,  23,  24,   6,   5,  10,
    0,  -9,  10,  17,  20,  15,   3,   6,
    0,   0,   0,   0,   0,   0,   0,   0
}};

const std::array<int, N_SQUARES> KNIGHT_BONUS_MG = {{
  -141,  -74,  -60,  -59,  -59,  -60,  -74, -141,
   -62,  -33,  -22,  -12,  -12,  -22,  -33,  -62,
   -49,  -14,    5,   10,   10,    5,  -14,  -49,
   -28,    6,   32,   39,   39,   32,    6,  -28,
   -27,   10,   35,   41,   41,   35,   10,  -27,
    -7,   18,   47,   43,   43,   47,   18,   -7,
   -54,  -22,    3,   30,   30,    3,  -22,  -54,
  -162,  -67,  -45,  -21,  -21,  -45,  -67, -162
}};

const std::array<int, N_SQUARES> KNIGHT_BONUS_EG = {{
   -77,  -52,  -40,  -17,  -17,  -40,  -52,  -77,
   -54,  -44,  -15,    6,    6,  -15,  -44,  -54,
   -32,  -22,   -6,   23,   23,   -6,  -22,  -32,
   -28,   -2,   10,   23,   23,   10,   -2,  -28,
   -36,  -13,    7,   31,   31,    7,  -13,  -36,
   -41,  -35,  -13,   14,   14,  -13,  -35,  -41,
   -55,  -40,  -41,   10,   10,  -41,  -40,  -55,
   -81,  -71,  -45,  -14,  -14,  -45,  -71,  -81
}};

const std::array<int, N_SQUARES> BISHOP_BONUS_MG = {{
   -43,   -4,   -6,  -19,  -19,   -6,   -4,  -43,
   -12,    6,   15,    3,    3,   15,    6,  -12,
    -6,   17,   -4,   14,   14,   -4,   17,   -6,
    -4,    9,   20,   31,   31,   20,    9,   -4,
   -10,   23,   18,   25,   25,   18,   23,  -10,
   -13,    5,    1,    9,    9,    1,    5,  -13,
   -14,  -11,    4,    0,    0,    4,  -11,  -14,
   -39,    1,  -11,  -19,  -19,  -11,    1,  -39
}};

const std::array<int, N_SQUARES> BISHOP_BONUS_EG = {{
   -46,  -24,  -30,  -10,  -10,  -30,  -24,  -46,
   -30,  -10,  -14,    1,    1,  -14,  -10,  -30,
   -13,   -1,   -2,    8,    8,   -2,   -1,  -13,
   -16,   -5,    0,   14,   14,    0,   -5,  -16,
   -14,   -1,  -11,   12,   12,  -11,   -1,  -14,
   -24,    5,    3,    5,    5,    3,    5,  -24,
   -25,  -16,   -1,    1,    1,   -1,  -16,  -25,
   -37,  -34,  -30,  -19,  -19,  -30,  -34,  -37
}};

const std::array<int, N_SQUARES> ROOK_BONUS_MG = {{
   -25,  -16,  -11,   -4,   -4,  -11,  -16,  -25,
   -17,  -10,   -6,    5,    5,   -6,  -10,  -17,
   -20,   -9,   -1,    2,    2,   -1,   -9,  -20,
   -10,   -4,   -3,   -5,   -5,   -3,   -4,  -10,
   -22,  -12,   -3,    2,    2,   -3,  -12,  -22,
   -18,   -2,    5,   10,   10,    5,   -2,  -18,
    -2,   10,   13,   15,   15,   13,   10,   -2,
   -14,  -15,   -1,    7,    7,   -1,  -15,  -14
}};

const std::array<int, N_SQUARES> ROOK_BONUS_EG = {{
    -7,  -10,   -8,   -7,   -7,   -8,  -10,   -7,
   -10,   -7,   -1,   -2,   -2,   -1,   -7,  -10,
     5,   -6,   -2,   -5,   -5,   -2,   -6,    5,
    -5,    1,   -7,    6,    6,   -7,    1,   -5,
    -4,    6,    6,   -5,   -5,    6,    6,   -4,
     5,    1,   -6,    8,    8,   -6,    1,    5,
     3,    4,   16,   -4,   -4,   16,    4,    3,
    15,    0,   15,   10,   10,   15,    0,   15
}};

const std::array<int, N_SQUARES> QUEEN_BONUS_MG = {{
     2,   -4,   -4,    3,    3,   -4,   -4,    2,
    -2,    4,    6,   10,   10,    6,    4,   -2,
    -2,    5,   10,    6,    6,   10,    5,   -2,
     3,    4,    7,    6,    6,    7,    4,    3,
     0,   11,   10,    4,    4,   10,   11,    0,
    -3,    8,    5,    6,    6,    5,    8,   -3,
    -4,    5,    8,    6,    6,    8,    5,   -4,
    -2,   -2,    1,   -2,   -2,    1,   -2,   -2
}};

const std::array<int, N_SQUARES> QUEEN_BONUS_EG = {{
   -56,  -46,  -38,  -21,  -21,  -38,  -46,  -56,
   -44,  -25,  -18,   -3,   -3,  -18,  -25,  -44,
   -31,  -15,   -7,    2,    2,   -7,  -15,  -31,
   -19,   -2,   10,   19,   19,   10,   -2,  -19,
   -23,   -5,    7,   17,   17,    7,   -5,  -23,
   -31,  -15,  -10,    1,    1,  -10,  -15,  -31,
   -40,  -22,  -19,   -6,   -6,  -19,  -22,  -40,
   -60,  -42,  -35,  -29,  -29,  -35,  -42,  -60
}};

const std::array<int, N_SQUARES> KING_BONUS_MG = {{
   219, 264, 219, 160, 160, 219, 264, 219,
   224, 244, 189, 144, 144, 189, 244, 224,
   157, 208, 136,  97,  97, 136, 208, 157,
   132, 153, 111,  79,  79, 111, 153, 132,
   124, 144,  85,  56,  56,  85, 144, 124,
    99, 117,  65,  25,  25,  65, 117,  99,
    71,  97,  52,  27,  27,  52,  97,  71,
    47,  72,  36,  -1,  -1,  36,  72,  47
}};

const std::array<int, N_SQUARES> KING_BONUS_EG = {{
     1,  36,  69,  61,  61,  69,  36,   1,
    43,  81, 107, 109, 109, 107,  81,  43,
    71, 105, 136, 141, 141, 136, 105,  71,
    83, 126, 138, 138, 138, 138, 126,  83,
    77, 133, 160, 160, 160, 160, 133,  77,
    74, 139, 148, 153, 153, 148, 139,  74,
    38,  98,  93, 106, 106,  93,  98,  38,
     9,  47,  59,  63,  63,  59,  47,   9
}};

const std::array<std::array<std::array<int, N_SQUARES>, N_PHASES>, 6> PSQ_VALUES = {{
    {PAWN_BONUS_MG, PAWN_BONUS_EG},
    {KNIGHT_BONUS_MG, KNIGHT_BONUS_EG},
    {BISHOP_BONUS_MG, BISHOP_BONUS_EG},
    {ROOK_BONUS_MG, ROOK_BONUS_EG},
    {QUEEN_BONUS_MG, QUEEN_BONUS_EG},
    {KING_BONUS_MG, KING_BONUS_EG}
}};

const Square SQUARE_MAP[N_COLORS][N_SQUARES] = {{
    H8, G8, F8, E8, D8, C8, B8, A8,
    H7, G7, F7, E7, D7, C7, B7, A7,
    H6, G6, F6, E6, D6, C6, B6, A6,
    H5, G5, F5, E5, D5, C5, B5, A5,
    H4, G4, F4, E4, D4, C4, B4, A4,
    H3, G3, F3, E3, D3, C3, B3, A3,
    H2, G2, F2, E2, D2, C2, B2, A2,
    H1, G1, F1, E1, D1, C1, B1, A1,
}, {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
}};
// clang-format on
