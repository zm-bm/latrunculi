#ifndef LATRUNCULI_MOVEGEN_H
#define LATRUNCULI_MOVEGEN_H

#include <iostream>
#include <vector>

#include "bb.hpp"
#include "magics.hpp"
#include "types.hpp"
#include "move.hpp"

class Chess;
class Move;

class MoveGen {
   public:
    MoveGen(Chess* chess) : chess(chess) {}

    void run();
    void runq();

    std::vector<Move> moves;

   private:
    Chess* chess;

    template <MoveGenType>
    void generate(U64);

    template <MoveGenType, Color>
    void generatePawnMoves(U64, U64);

    template <MoveGenType>
    void generateKingMoves(U64, U64);

    template <PieceRole, Color>
    void generatePieceMoves(U64, U64);

    void generateEvasions();
    void generateCastling();

    template <PawnMove, Color>
    void appendPawnMoves(U64);
    template <PawnMove, Color, MoveGenType>
    void appendPawnPromotions(U64);
};

#endif
