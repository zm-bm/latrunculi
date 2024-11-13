#ifndef LATRUNCULI_EVAL_H
#define LATRUNCULI_EVAL_H

#include "bb.hpp"
#include "types.hpp"

namespace Eval {

const int midgameLimit = 15258;
const int endgameLimit = 3915;

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

}

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

#endif