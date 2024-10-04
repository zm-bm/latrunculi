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

namespace MoveGen {
class Generator {
   public:
    Generator(Chess* chess) : chess(chess) {}

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

template <PieceRole p>
inline int mobility(U64 bitboard, U64 targets, U64 occ) {
    int nmoves = 0;

    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square from = BB::lsb(bitboard);
        bitboard &= BB::clear(from);

        U64 mobility = BB::movesByPiece<p>(from, occ) & targets;
        nmoves += BB::bitCount(mobility);
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

}  // namespace MoveGen

#endif
