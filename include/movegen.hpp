#ifndef LATRUNCULI_MOVEGEN_H
#define LATRUNCULI_MOVEGEN_H

#include <iostream>
#include <vector>

#include "bitboard.hpp"
#include "magics.hpp"
#include "types.hpp"

class Board;
class Move;

namespace MoveGen {
class Generator {
   public:
    Generator(Board* board) : b(board) {}

    void run();
    void runq();

    std::vector<Move> moves;

   private:
    Board* b;

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

template <PawnMove p, Color c>
inline U64 movesByPawns(U64 pawns) {
    if (p == PawnMove::LEFT)
        pawns &= ~G::filemask(FILE1, c);
    else if (p == PawnMove::RIGHT)
        pawns &= ~G::filemask(FILE8, c);

    if (c == WHITE)
        return pawns << static_cast<int>(p);
    else
        return pawns >> static_cast<int>(p);
}

template <PawnMove p>
inline U64 movesByPawns(U64 pawns, Color c) {
    if (c == WHITE)
        return movesByPawns<p, WHITE>(pawns);
    else
        return movesByPawns<p, BLACK>(pawns);
};
;

template <Color c>
inline U64 attacksByPawns(U64 pawns) {
    return movesByPawns<PawnMove::LEFT, c>(pawns) |
           movesByPawns<PawnMove::RIGHT, c>(pawns);
}

inline U64 attacksByPawns(U64 pawns, Color c) {
    return movesByPawns<PawnMove::LEFT>(pawns, c) |
           movesByPawns<PawnMove::RIGHT>(pawns, c);
}

template <PieceRole p>
inline U64 movesByPiece(Square sq, U64 occupancy) {
    switch (p) {
        case KNIGHT:
            return G::KNIGHT_ATTACKS[sq];
        case BISHOP:
            return Magics::getBishopAttacks(sq, occupancy);
        case ROOK:
            return Magics::getRookAttacks(sq, occupancy);
        case QUEEN:
            return Magics::getQueenAttacks(sq, occupancy);
        case KING:
            return G::KING_ATTACKS[sq];
    }
}

template <PieceRole p>
inline U64 movesByPiece(Square sq) {
    return movesByPiece<p>(sq, 0);
}

inline U64 movesByPiece(Square sq, PieceRole p, U64 occupancy) {
    switch (p) {
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
            return 0;
    }
}

inline U64 movesByPiece(Square sq, PieceRole p) {
    return movesByPiece(sq, p, 0);
}

template <PieceRole p>
inline int mobility(U64 bitboard, U64 targets, U64 occ) {
    int nmoves = 0;

    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square from = BB::lsb(bitboard);
        bitboard &= BB::clear(from);

        U64 mobility = movesByPiece<p>(from, occ) & targets;
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
