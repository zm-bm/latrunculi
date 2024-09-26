#ifndef LATRUNCULI_EVAL_H
#define LATRUNCULI_EVAL_H

#include "types.hpp"
#include "bitboard.hpp"

namespace Eval
{
    extern const int MobilityScaling[2][6];

    extern const int PassedPawnBonus[2];
    extern const int DoublePawnPenalty[2];
    extern const int TriplePawnPenalty[2];
    extern const int IsoPawnPenalty[2];
    extern const int OpenFileBonus[2];
    extern const int HalfOpenFileBonus[2];
    extern const int BishopPairBonus[2];

    extern const int PieceValues[6][2];
    extern const int PieceSqValues[6][2][64];

    extern const int TEMPO_BONUS;
    extern const int KNIGHT_PENALTY_PER_PAWN;
    extern const int ROOK_BONUS_PER_PAWN;
    extern const int CONNECTED_ROOK_BONUS;
    extern const int ROOK_ON_SEVENTH_BONUS;
    extern const int BACK_RANK_MINOR_PENALTY;
    extern const int MINOR_OUTPOST_BONUS;
    extern const int STRONG_KING_SHIELD_BONUS;
    extern const int WEAK_KING_SHIELD_BONUS;

    extern const int KNIGHT_TROPISM[8];
    extern const int BISHOP_TROPISM[8];
    extern const int ROOK_TROPISM[8];
    extern const int QUEEN_TROPISM[8];

    extern const Square ColorSq[2][64];

    inline int psqv(Color c, PieceRole pt, int phase, Square sq)
    {
        // Get the piece square value
        int score = PieceSqValues[pt-1][phase][ColorSq[c][sq]];
        // Return +score for white, -score for black
        return (2 * c * score) - score;
    }

    template<Color c>
    inline BBz kingShield(Square sq)
    {
        BBz bitboard = BBz(G::BITSET[sq]);
        if (c)
            return bitboard.shift_nw()
                 | bitboard.shift_no()
                 | bitboard.shift_ne();

        else
            return bitboard.shift_sw()
                 | bitboard.shift_so()
                 | bitboard.shift_se();
    }
}

#endif