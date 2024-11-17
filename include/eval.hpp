#ifndef LATRUNCULI_EVAL_H
#define LATRUNCULI_EVAL_H

#include <array>

#include "bb.hpp"
#include "types.hpp"

namespace Eval {

const int PHASE_LIMIT = 128;
const int MG_LIMIT = 15258;
const int EG_LIMIT = 3915;
const int TEMPO_BONUS = 25;

const int mgPieceValue[N_PIECES - 1] = {0, 124, 781, 825, 1276, 2538};
const int egPieceValue[N_PIECES - 1] = {0, 206, 854, 915, 1380, 2682};

const U64 WHITESQUARES = 0x55AA55AA55AA55AA;
const U64 BLACKSQUARES = 0xAA55AA55AA55AA55;
const U64 WHITEHOLES = 0x0000003CFFFF0000;
const U64 BLACKHOLES = 0x0000FFFF3C000000;

inline int taperScore(int mgScore, int egScore, int phase) {
    return (mgScore * phase + egScore * (PHASE_LIMIT - phase)) / PHASE_LIMIT;
}

inline int calculatePhase(int nonPawnMaterial) {
    nonPawnMaterial = std::clamp(nonPawnMaterial, Eval::EG_LIMIT, Eval::MG_LIMIT);
    return ((nonPawnMaterial - Eval::EG_LIMIT) * PHASE_LIMIT) / (Eval::MG_LIMIT - Eval::EG_LIMIT);
}

inline int tempoBonus(Color c) {
    return c == WHITE ? TEMPO_BONUS : -TEMPO_BONUS;
}

// clang-format off
constexpr std::array<int, 64> pawnBonusMg = {{
     0,   0,   0,   0,   0,   0,   0,   0,
     3,   3,  10,  19,  16,  19,   7,  -5,
    -9, -15,  11,  15,  32,  22,   5, -22,
    -4, -23,   6,  20,  40,  17,   4,  -8,
    13,   0, -13,   1,  11,  -2, -13,   5,
     5, -12,  -7,  22,  -8,  -5, -15,  -8,
    -7,   7,  -3, -13,   5, -16,  10,  -8,
     0,   0,   0,   0,   0,   0,   0,   0
}};

constexpr std::array<int, 64> knightBonusMg = {{
   -175, -92, -74, -73, -73, -74, -92, -175,
    -77, -41, -27, -15, -15, -27, -41,  -77,
    -61, -17,   6,  12,  12,   6, -17,  -61,
    -35,   8,  40,  49,  49,  40,   8,  -35,
    -34,  13,  44,  51,  51,  44,  13,  -34,
     -9,  22,  58,  53,  53,  58,  22,   -9,
    -67, -27,   4,  37,  37,   4, -27,  -67,
   -201, -83, -56, -26, -26, -56, -83, -201
}};

constexpr std::array<int, 64> bishopBonusMg = {{
    -53,  -5,  -8, -23, -23,  -8,  -5, -53,
    -15,   8,  19,   4,   4,  19,   8, -15,
     -7,  21,  -5,  17,  17,  -5,  21,  -7,
     -5,  11,  25,  39,  39,  25,  11,  -5,
    -12,  29,  22,  31,  31,  22,  29, -12,
    -16,   6,   1,  11,  11,   1,   6, -16,
    -17, -14,   5,   0,   0,   5, -14, -17,
    -48,   1, -14, -23, -23, -14,   1, -48
}};

constexpr std::array<int, 64> rookBonusMg = {{
    -31, -20, -14,  -5,  -5, -14, -20, -31,
    -21, -13,  -8,   6,   6,  -8, -13, -21,
    -25, -11,  -1,   3,   3,  -1, -11, -25,
    -13,  -5,  -4,  -6,  -6,  -4,  -5, -13,
    -27, -15,  -4,   3,   3,  -4, -15, -27,
    -22,  -2,   6,  12,  12,   6,  -2, -22,
     -2,  12,  16,  18,  18,  16,  12,  -2,
    -17, -19,  -1,   9,   9,  -1, -19, -17
}};

constexpr std::array<int, 64> queenBonusMg = {{
      3,  -5,  -5,   4,   4,  -5,  -5,   3,
     -3,   5,   8,  12,  12,   8,   5,  -3,
     -3,   6,  13,   7,   7,  13,   6,  -3,
      4,   5,   9,   8,   8,   9,   5,   4,
      0,  14,  12,   5,   5,  12,  14,   0,
     -4,  10,   6,   8,   8,   6,  10,  -4,
     -5,   6,  10,   8,   8,  10,   6,  -5,
     -2,  -2,   1,  -2,  -2,   1,  -2,  -2
}};

constexpr std::array<int, 64> kingBonusMg = {{
    271, 327, 271, 198, 198, 271, 327, 271,
    278, 303, 234, 179, 179, 234, 303, 278,
    195, 258, 169, 120, 120, 169, 258, 195,
    164, 190, 138,  98,  98, 138, 190, 164,
    154, 179, 105,  70,  70, 105, 179, 154,
    123, 145,  81,  31,  31,  81, 145, 123,
     88, 120,  65,  33,  33,  65, 120,  88,
     59,  89,  45,  -1,  -1,  45,  89,  59
}};

const Square ColorSq[2][64] = {
    {
        A1, B1, C1, D1, E1, F1, G1, H1,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A8, B8, C8, D8, E8, F8, G8, H8,
    }, {
        H8, G8, F8, E8, D8, C8, B8, A8,
        H7, G7, F7, E7, D7, C7, B7, A7,
        H6, G6, F6, E6, D6, C6, B6, A6,
        H5, G5, F5, E5, D5, C5, B5, A5,
        H4, G4, F4, E4, D4, C4, B4, A4,
        H3, G3, F3, E3, D3, C3, B3, A3,
        H2, G2, F2, E2, D2, C2, B2, A2,
        H1, G1, F1, E1, D1, C1, B1, A1,
    }
};

// clang-format off

// const std::array<std::array<int, 64>, 2> PawnSqValues = {{
//     {
//         0,  0,  0,  0,  0,  0,  0,  0,
//         50, 50, 50, 50, 50, 50, 50, 50,
//         10, 10, 20, 30, 30, 20, 10, 10,
//         5,  5, 10, 25, 25, 10,  5,  5,
//         0,  0,  0, 20, 20,  0,  0,  0,
//         5, -5,-10,  0,  0,-10, -5,  5,
//         5, 10, 10,-20,-20, 10, 10,  5,
//         0,  0,  0,  0,  0,  0,  0,  0
//     }, {
//         0,  0,  0,  0,  0,  0,  0,  0,
//         115,125,125,125,125,125,125,125,
//         85, 95, 95,105,105, 95, 95, 85,
//         75, 85, 90,100,100, 90, 85, 65,
//         65, 80, 80, 95, 95, 80, 80, 65,
//         55, 75, 75, 75, 75, 75, 75, 55,
//         50, 70, 70, 70, 70, 70, 70, 50,
//         0,  0,  0,  0,  0,  0,  0,  0
//     }
// }};

// const std::array<std::array<int, 64>, 2> KnightSqValues = {{
//     {
//         -50,-40,-30,-30,-30,-30,-40,-50,
//         -40,-20,  0,  0,  0,  0,-20,-40,
//         -30,  0, 10, 15, 15, 10,  0,-30,
//         -30,  5, 15, 20, 20, 15,  5,-30,
//         -30,  0, 15, 20, 20, 15,  0,-30,
//         -30,  5, 10, 15, 15, 10,  5,-30,
//         -40,-20,  0,  5,  5,  0,-20,-40,
//         -50,-40,-30,-30,-30,-30,-40,-50,
//     }, {
//         -50,-40,-30,-30,-30,-30,-40,-50,
//         -40,-20,  0,  0,  0,  0,-20,-40,
//         -30,  0, 10, 15, 15, 10,  0,-30,
//         -30,  5, 15, 20, 20, 15,  5,-30,
//         -30,  0, 15, 20, 20, 15,  0,-30,
//         -30,  5, 10, 15, 15, 10,  5,-30,
//         -40,-20,  0,  5,  5,  0,-20,-40,
//         -50,-40,-30,-30,-30,-30,-40,-50,
//     }
// }};

// const std::array<std::array<int, 64>, 2> BishopSqValues = {{
//     {
//         -20,-10,-10,-10,-10,-10,-10,-20,
//         -10,  0,  0,  0,  0,  0,  0,-10,
//         -10,  0,  5, 10, 10,  5,  0,-10,
//         -10,  5,  5, 10, 10,  5,  5,-10,
//         -10,  0, 10, 10, 10, 10,  0,-10,
//         -10, 10, 10, 10, 10, 10, 10,-10,
//         -10, 10,  0,  0,  0,  0, 10,-10,
//         -20,-10,-10,-10,-10,-10,-10,-20
//     }, {
//         -20,-10,-10,-10,-10,-10,-10,-20,
//         -10,  0,  0,  0,  0,  0,  0,-10,
//         -10,  0,  5, 10, 10,  5,  0,-10,
//         -10,  5,  5, 10, 10,  5,  5,-10,
//         -10,  0, 10, 10, 10, 10,  0,-10,
//         -10, 10, 10, 10, 10, 10, 10,-10,
//         -10,  5,  0,  0,  0,  0,  5,-10,
//         -20,-10,-10,-10,-10,-10,-10,-20
//     }
// }};

// const std::array<std::array<int, 64>, 2> RookSqValues = {{
//     {
//         0,  0,  0,  0,  0,  0,  0,  0,
//         5, 10, 10, 10, 10, 10, 10,  5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         0,  0,  0,  5,  5,  0,  0,  0
//     }, {
//         0,  0,  0,  0,  0,  0,  0,  0,
//         5, 10, 10, 10, 10, 10, 10,  5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         -5,  0,  0,  0,  0,  0,  0, -5,
//         0,  0,  0,  5,  5,  0,  0,  0
//     }
// }};

// const std::array<std::array<int, 64>, 2> QueenSqValues = {{
//     {
//         -20,-10,-10, -5, -5,-10,-10,-20,
//         -10,  0,  0,  0,  0,  0,  0,-10,
//         -10,  0,  5,  5,  5,  5,  0,-10,
//         -5,  0,  5,  5,  5,  5,  0, -5,
//         0,  0,  5,  5,  5,  5,  0, -5,
//         -10,  5,  5,  5,  5,  5,  0,-10,
//         -10,  0,  5,  0,  0,  0,  0,-10,
//         -20,-10,-10, -5, -5,-10,-10,-20
//     }, {
//         -20,-10,-10, -5, -5,-10,-10,-20,
//         -10,  0,  0,  0,  0,  0,  0,-10,
//         -10,  0,  5,  5,  5,  5,  0,-10,
//         -5,  0,  5,  5,  5,  5,  0, -5,
//         0,  0,  5,  5,  5,  5,  0, -5,
//         -10,  5,  5,  5,  5,  5,  0,-10,
//         -10,  0,  5,  0,  0,  0,  0,-10,
//         -20,-10,-10, -5, -5,-10,-10,-20
//     }
// }};

// const std::array<std::array<int, 64>, 2> KingSqValues = {{
//     {
//         30,-40,-40,-50,-50,-40,-40,-30,
//         -30,-40,-40,-50,-50,-40,-40,-30,
//         -30,-40,-40,-50,-50,-40,-40,-30,
//         -30,-40,-40,-50,-50,-40,-40,-30,
//         -20,-30,-30,-40,-40,-30,-30,-20,
//         -10,-20,-20,-20,-20,-20,-20,-10,
//         20, 20,  0,  0,  0,  0, 20, 20,
//         20, 30, 10,  0,  0, 10, 30, 20
//     }, {
//         -50,-40,-30,-20,-20,-30,-40,-50,
//         -30,-20,-10,  0,  0,-10,-20,-30,
//         -30,-10, 20, 30, 30, 20,-10,-30,
//         -30,-10, 30, 40, 40, 30,-10,-30,
//         -30,-10, 30, 40, 40, 30,-10,-30,
//         -30,-10, 20, 30, 30, 20,-10,-30,
//         -30,-30,  0,  0,  0,  0,-30,-30,
//         -50,-30,-30,-30,-30,-30,-30,-50
//     }
// }};

// const std::array<std::array<std::array<int, 64>, 2>, 6> PieceSqValues = {{
//     PawnSqValues,
//     KnightSqValues,
//     BishopSqValues,
//     RookSqValues,
//     QueenSqValues,
//     KingSqValues,
// }};

// inline int psqv(Color c, PieceType p, int phase, Square sq) {
//     // Get the piece square value for color c
//     int score = PieceSqValues[p - 1][phase][ColorSq[c][sq]];
//     return (2 * c * score) - score;
// }

}  // namespace Eval

// int calculateMobilityScore(const int, const int) const;
// template <PieceType>
// int calculateMobilityScore(const int, const int) const;
// template <PieceType p>
// int calculateMobility(U64 bitboard, U64 targets, U64 occ) const;

// int Board::calculateMobilityScore(const int opPhase, const int egPhase) const
// {
//     int score = 0;
//     score += calculateMobilityScore<KNIGHT>(opPhase, egPhase);
//     score += calculateMobilityScore<BISHOP>(opPhase, egPhase);
//     score += calculateMobilityScore<ROOK  >(opPhase, egPhase);
//     score += calculateMobilityScore<QUEEN >(opPhase, egPhase);
//     return score;
// }

// template<PieceType p>
// int Board::calculateMobilityScore(const int opPhase, const int egPhase) const
// {
//     int mobilityScore = 0;
//     U64 occ = occupancy();

//     int whitemoves = calculateMobility<p>(getPieces<p>(WHITE),
//                                           ~getPieces<ALL_PIECE_TYPES>(WHITE), occ);
//     mobilityScore += whitemoves * (opPhase * Eval::MobilityScaling[MIDGAME][p]
//                                  + egPhase * Eval::MobilityScaling[ENDGAME][p]);

//     int blackmoves = calculateMobility<p>(getPieces<p>(BLACK),
//                                           ~getPieces<ALL_PIECE_TYPES>(BLACK), occ);
//     mobilityScore -= blackmoves * (opPhase * Eval::MobilityScaling[MIDGAME][p]
//                                  + egPhase * Eval::MobilityScaling[ENDGAME][p]);
//     return mobilityScore;
// }

// template <PieceType p>
// int Board::calculateMobility(U64 bitboard, U64 targets, U64 occ) const {
//     int nmoves = 0;

//     while (bitboard) {
//         // Pop lsb bit and clear it from the bitboard
//         Square from = BB::lsb(bitboard);
//         bitboard &= BB::clear(from);

//         U64 mobility = BB::movesByPiece<p>(from, occ) & targets;
//         nmoves += BB::bitCount(mobility);
//     }

//     return nmoves;
// }

// clang-format off

// clang-format on

#endif