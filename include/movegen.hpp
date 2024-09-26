#ifndef LATRUNCULI_MOVEGEN_H
#define LATRUNCULI_MOVEGEN_H

#include <iostream>
#include <vector>
#include "types.hpp"
#include "bitboard.hpp"
#include "magics.hpp"

class Board;
class Move;

namespace MoveGen
{
    class Generator
    {
    public:

        Generator(Board * board)
        : b(board)
        { }

        void run();
        void runq();

        std::vector<Move> moves;

    private:
        
        Board * b;

        template<MoveGenType>        void generate(BBz);
        template<MoveGenType, Color> void generatePawnMoves(BBz, BBz);
        template<MoveGenType>        void generateKingMoves(BBz, BBz);
        template<PieceRole, Color>   void generatePieceMoves(BBz, BBz);
                                     void generateEvasions();
                                     void generateCastling();

        template<PawnMove, Color>               void appendPawnMoves(BBz);
        template<PawnMove, Color, MoveGenType>  void appendPawnPromotions(BBz);

    };

    template<PawnMove p, Color c>
    inline BBz movesByPawns(BBz pawns)
    {
        if (p == PawnMove::LEFT)
            pawns &= ~G::filemask(FILE1, c);
        else if (p == PawnMove::RIGHT)
            pawns &= ~G::filemask(FILE8, c);

        if (c == WHITE)
            return pawns << static_cast<int>(p);
        else
            return pawns >> static_cast<int>(p);
    }
    
    template<PawnMove p>
    inline BBz movesByPawns(BBz pawns, Color c)
    {
        if (c == WHITE)
            return movesByPawns<p, WHITE>(pawns);
        else
            return movesByPawns<p, BLACK>(pawns);
    };;

    template<Color c>
    inline BBz attacksByPawns(BBz pawns)
    {
        return movesByPawns<PawnMove::LEFT,  c>(pawns)
             | movesByPawns<PawnMove::RIGHT, c>(pawns);
    }

    inline BBz attacksByPawns(BBz pawns, Color c)
    {
        return movesByPawns<PawnMove::LEFT >(pawns, c)
             | movesByPawns<PawnMove::RIGHT>(pawns, c);
    }

    template<PieceRole p>
    inline BBz movesByPiece(Square sq, BBz occupancy)
    {
        switch (p)
        {
            case KNIGHT:
                return G::KNIGHT_ATTACKS[sq];
            case BISHOP:
                return BBz(Magics::getBishopAttacks(sq, occupancy));
            case ROOK:
                return BBz(Magics::getRookAttacks(sq, occupancy));
            case QUEEN:
                return BBz(Magics::getQueenAttacks(sq, occupancy));
            case KING:
                return G::KING_ATTACKS[sq];
        }
    }

    template<PieceRole p>
    inline BBz movesByPiece(Square sq)
    {
        return movesByPiece<p>(sq, BBz(0));
    }

    inline BBz movesByPiece(Square sq, PieceRole p, BBz occupancy)
    {
        switch (p)
        {
            case KNIGHT:
                return movesByPiece<KNIGHT>(sq, occupancy);
            case BISHOP:
                return movesByPiece<BISHOP>(sq, occupancy);
            case ROOK:
                return movesByPiece<ROOK>(sq, occupancy);
            case QUEEN:
                return movesByPiece<QUEEN>(sq, occupancy);
            case KING:
                return movesByPiece<KING>(sq, occupancy);
            default:
                return BBz(0x0);
        }
    }

    inline BBz movesByPiece(Square sq, PieceRole p)
    {
        return movesByPiece(sq, p, BBz(0));
    }

    template<PieceRole p>
    inline int mobility(BBz bitboard, BBz targets, BBz occ)
    {
        int nmoves = 0;

        while (bitboard)
        {
            // Pop lsb bit and clear it from the bitboard
            Square from = bitboard.lsb();
            bitboard.clear(from);

            BBz mobility = movesByPiece<p>(from, occ) & targets;
            nmoves += mobility.count();
        }

        return nmoves;
    }

    extern const U64 CastlePathOO[];
    extern const U64 CastlePathOOO[];
    extern const U64 KingCastlePathOO[];
    extern const U64 KingCastlePathOOO[];

    extern const Square KingOrigin[];
    extern const Square KingDestinationOO[];
    extern const Square KingDestinationOOO[];
    extern const Square RookOriginOO[];
    extern const Square RookOriginOOO[];

}

    

#endif
