#ifndef ANTONIUS_MOVEGEN_H
#define ANTONIUS_MOVEGEN_H

#include <iostream>
#include <vector>
#include "types.hpp"
#include "bitboard.hpp"

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

        template<MoveGenType>        void generate(BB);
        template<MoveGenType, Color> void generatePawnMoves(BB, BB);
        template<MoveGenType>        void generateKingMoves(BB, BB);
        template<PieceType, Color>   void generatePieceMoves(BB, BB);
                                     void generateEvasions();
                                     void generateCastling();

        template<PawnMove, Color>               void appendPawnMoves(BB);
        template<PawnMove, Color, MoveGenType>  void appendPawnPromotions(BB);

    };

    template<PawnMove p, Color c>
    inline BB movesByPawns(BB pawns)
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
    inline BB movesByPawns(BB pawns, Color c)
    {
        if (c == WHITE)
            return movesByPawns<p, WHITE>(pawns);
        else
            return movesByPawns<p, BLACK>(pawns);
    };;

    template<Color c>
    inline BB attacksByPawns(BB pawns)
    {
        return movesByPawns<PawnMove::LEFT,  c>(pawns)
             | movesByPawns<PawnMove::RIGHT, c>(pawns);
    }

    inline BB attacksByPawns(BB pawns, Color c)
    {
        return movesByPawns<PawnMove::LEFT >(pawns, c)
             | movesByPawns<PawnMove::RIGHT>(pawns, c);
    }

    template<PieceType p>
    inline BB movesByPiece(Square sq, BB occupancy)
    {
        switch (p)
        {
            case KNIGHT:
                return G::KNIGHT_ATTACKS[sq];
            case BISHOP:
                return BB(Magic::getBishopAttacks(sq, occupancy));
            case ROOK:
                return BB(Magic::getRookAttacks(sq, occupancy));
            case QUEEN:
                return BB(Magic::getQueenAttacks(sq, occupancy));
            case KING:
                return G::KING_ATTACKS[sq];
        }
    }

    template<PieceType p>
    inline BB movesByPiece(Square sq)
    {
        return movesByPiece<p>(sq, BB(0));
    }

    inline BB movesByPiece(Square sq, PieceType p, BB occupancy)
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
                return BB(0x0);
        }
    }

    inline BB movesByPiece(Square sq, PieceType p)
    {
        return movesByPiece(sq, p, BB(0));
    }

    template<PieceType p>
    inline int mobility(BB bitboard, BB targets, BB occ)
    {
        int nmoves = 0;

        while (bitboard)
        {
            // Pop lsb bit and clear it from the bitboard
            Square from = bitboard.lsb();
            bitboard.clear(from);

            BB mobility = movesByPiece<p>(from, occ) & targets;
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
