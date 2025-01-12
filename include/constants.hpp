#ifndef LATRUNCULI_CONSTANTS_H
#define LATRUNCULI_CONSTANTS_H

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
           POS5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
const std::string FENS[6] = {STARTFEN, POS2, POS3, POS4W, POS4B, POS5};

// more testing FENs
const auto EMPTYFEN = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";
const auto E2PAWN = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";
const auto E4PAWN = "4k3/8/8/8/4P3/8/8/4K3 w - - 0 1";
const auto A3ENPASSANT = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1";
const auto A7PAWN = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
const auto BLACKMOVE = "4k3/8/8/8/8/8/8/4K3 b - - 0 1";

// move gen constants
const Square KingOrigin[2] = {E8, E1};
const Square KingDestinationOO[2] = {G8, G1};
const Square KingDestinationOOO[2] = {C8, C1};
const Square RookOriginOO[2] = {H8, H1};
const Square RookOriginOOO[2] = {A8, A1};

// eval constants
const int SCALE_LIMIT = 64;
const int PHASE_LIMIT = 128;
const int MG_LIMIT = 15258;
const int EG_LIMIT = 3915;
const int TEMPO_BONUS = 25;
const int ISO_PAWN_PENALTY[N_PHASES] = {-5, -15};
const int BACKWARD_PAWN_PENALTY[N_PHASES] = {-9, -25};
const int DOUBLED_PAWN_PENALTY[N_PHASES] = {-11, -56};

const U64 WHITE_SQUARES = 0x55AA55AA55AA55AA;
const U64 BLACK_SQUARES = 0xAA55AA55AA55AA55;
const U64 WHITE_OUTPOSTS = 0x0000FFFFFF000000;
const U64 BLACK_OUTPOSTS = 0x000000FFFFFF0000;

const int PAWN_VALUE_MG = 124;
const int KNIGHT_VALUE_MG = 781;
const int BISHOP_VALUE_MG = 825;
const int ROOK_VALUE_MG = 1276;
const int QUEEN_VALUE_MG = 2538;
const int PAWN_VALUE_EG = 206;
const int KNIGHT_VALUE_EG = 854;
const int BISHOP_VALUE_EG = 915;
const int ROOK_VALUE_EG = 1380;
const int QUEEN_VALUE_EG = 2682;

// clang-format off
const int PIECE_VALUES[N_PHASES][N_COLORS][N_PIECES] = {{
    {0, -PAWN_VALUE_MG, -KNIGHT_VALUE_MG, -BISHOP_VALUE_MG, -ROOK_VALUE_MG, -QUEEN_VALUE_MG, 0},
    {0, PAWN_VALUE_MG, KNIGHT_VALUE_MG, BISHOP_VALUE_MG, ROOK_VALUE_MG, QUEEN_VALUE_MG, 0}       
}, {
    {0, -PAWN_VALUE_EG, -KNIGHT_VALUE_EG, -BISHOP_VALUE_EG, -ROOK_VALUE_EG, -QUEEN_VALUE_EG, 0},
    {0, PAWN_VALUE_EG, KNIGHT_VALUE_EG, BISHOP_VALUE_EG, ROOK_VALUE_EG, QUEEN_VALUE_EG, 0}
}};

const ScoreArray PAWN_BONUS_MG = {{
     0,   0,   0,   0,   0,   0,   0,   0,
     3,   3,  10,  19,  16,  19,   7,  -5,
    -9, -15,  11,  15,  32,  22,   5, -22,
    -4, -23,   6,  20,  40,  17,   4,  -8,
    13,   0, -13,   1,  11,  -2, -13,   5,
     5, -12,  -7,  22,  -8,  -5, -15,  -8,
    -7,   7,  -3, -13,   5, -16,  10,  -8,
     0,   0,   0,   0,   0,   0,   0,   0
}};

const ScoreArray PAWN_BONUS_EG = {{
     0,   0,   0,   0,   0,   0,   0,   0,
   -10,  -6,  10,   0,  14,   7,  -5, -19,
   -10, -10, -10,   4,   4,   3,  -6,  -4,
     6,  -2,  -8,  -4, -13, -12, -10,  -9,
    10,   5,   4,  -5,  -5,  -5,  14,   9,
    28,  20,  21,  28,  30,   7,   6,  13,
     0, -11,  12,  21,  25,  19,   4,   7,
     0,   0,   0,   0,   0,   0,   0,   0
}};

const ScoreArray KNIGHT_BONUS_MG = {{
   -175, -92, -74, -73, -73, -74, -92, -175,
    -77, -41, -27, -15, -15, -27, -41,  -77,
    -61, -17,   6,  12,  12,   6, -17,  -61,
    -35,   8,  40,  49,  49,  40,   8,  -35,
    -34,  13,  44,  51,  51,  44,  13,  -34,
     -9,  22,  58,  53,  53,  58,  22,   -9,
    -67, -27,   4,  37,  37,   4, -27,  -67,
   -201, -83, -56, -26, -26, -56, -83, -201
}};

const ScoreArray KNIGHT_BONUS_EG = {{
    -96, -65, -49, -21, -21, -49, -65, -96,
    -67, -54, -18,   8,   8, -18, -54, -67,
    -40, -27,  -8,  29,  29,  -8, -27, -40,
    -35,  -2,  13,  28,  28,  13,  -2, -35,
    -45, -16,   9,  39,  39,   9, -16, -45,
    -51, -44, -16,  17,  17, -16, -44, -51,
    -69, -50, -51,  12,  12, -51, -50, -69,
   -100, -88, -56, -17, -17, -56, -88, -100
}};

const ScoreArray BISHOP_BONUS_MG = {{
    -53,  -5,  -8, -23, -23,  -8,  -5, -53,
    -15,   8,  19,   4,   4,  19,   8, -15,
     -7,  21,  -5,  17,  17,  -5,  21,  -7,
     -5,  11,  25,  39,  39,  25,  11,  -5,
    -12,  29,  22,  31,  31,  22,  29, -12,
    -16,   6,   1,  11,  11,   1,   6, -16,
    -17, -14,   5,   0,   0,   5, -14, -17,
    -48,   1, -14, -23, -23, -14,   1, -48
}};

const ScoreArray BISHOP_BONUS_EG = {{
    -57, -30, -37, -12, -12, -37, -30, -57,
    -37, -13, -17,   1,   1, -17, -13, -37,
    -16,  -1,  -2,  10,  10,  -2,  -1, -16,
    -20,  -6,   0,  17,  17,   0,  -6, -20,
    -17,  -1, -14,  15,  15, -14,  -1, -17,
    -30,   6,   4,   6,   6,   4,   6, -30,
    -31, -20,  -1,   1,   1,  -1, -20, -31,
    -46, -42, -37, -24, -24, -37, -42, -46
}};

const ScoreArray ROOK_BONUS_MG = {{
    -31, -20, -14,  -5,  -5, -14, -20, -31,
    -21, -13,  -8,   6,   6,  -8, -13, -21,
    -25, -11,  -1,   3,   3,  -1, -11, -25,
    -13,  -5,  -4,  -6,  -6,  -4,  -5, -13,
    -27, -15,  -4,   3,   3,  -4, -15, -27,
    -22,  -2,   6,  12,  12,   6,  -2, -22,
     -2,  12,  16,  18,  18,  16,  12,  -2,
    -17, -19,  -1,   9,   9,  -1, -19, -17
}};

const ScoreArray ROOK_BONUS_EG = {{
     -9, -13, -10,  -9,  -9, -10, -13,  -9,
    -12,  -9,  -1,  -2,  -2,  -1,  -9, -12,
      6,  -8,  -2,  -6,  -6,  -2,  -8,   6,
     -6,   1,  -9,   7,   7,  -9,   1,  -6,
     -5,   8,   7,  -6,  -6,   7,   8,  -5,
      6,   1,  -7,  10,  10,  -7,   1,   6,
      4,   5,  20,  -5,  -5,  20,   5,   4,
     18,   0,  19,  13,  13,  19,   0,  18
}};

const ScoreArray QUEEN_BONUS_MG = {{
      3,  -5,  -5,   4,   4,  -5,  -5,   3,
     -3,   5,   8,  12,  12,   8,   5,  -3,
     -3,   6,  13,   7,   7,  13,   6,  -3,
      4,   5,   9,   8,   8,   9,   5,   4,
      0,  14,  12,   5,   5,  12,  14,   0,
     -4,  10,   6,   8,   8,   6,  10,  -4,
     -5,   6,  10,   8,   8,  10,   6,  -5,
     -2,  -2,   1,  -2,  -2,   1,  -2,  -2
}};

const ScoreArray QUEEN_BONUS_EG = {{
    -69, -57, -47, -26, -26, -47, -57, -69,
    -55, -31, -22,  -4,  -4, -22, -31, -55,
    -39, -18,  -9,   3,   3,  -9, -18, -39,
    -23,  -3,  13,  24,  24,  13,  -3, -23,
    -29,  -6,   9,  21,  21,   9,  -6, -29,
    -38, -18, -12,   1,   1, -12, -18, -38,
    -50, -27, -24,  -8,  -8, -24, -27, -50,
    -75, -52, -43, -36, -36, -43, -52, -75
}};

const ScoreArray KING_BONUS_MG = {{
    271, 327, 271, 198, 198, 271, 327, 271,
    278, 303, 234, 179, 179, 234, 303, 278,
    195, 258, 169, 120, 120, 169, 258, 195,
    164, 190, 138,  98,  98, 138, 190, 164,
    154, 179, 105,  70,  70, 105, 179, 154,
    123, 145,  81,  31,  31,  81, 145, 123,
     88, 120,  65,  33,  33,  65, 120,  88,
     59,  89,  45,  -1,  -1,  45,  89,  59
}};

const ScoreArray KING_BONUS_EG = {{
      1,  45,  85,  76,  76,  85,  45,   1,
     53, 100, 133, 135, 135, 133, 100,  53,
     88, 130, 169, 175, 175, 169, 130,  88,
    103, 156, 172, 172, 172, 172, 156, 103,
     96, 166, 199, 199, 199, 199, 166,  96,
     92, 172, 184, 191, 191, 184, 172,  92,
     47, 121, 116, 131, 131, 116, 121,  47,
     11,  59,  73,  78,  78,  73,  59,  11
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

#endif
