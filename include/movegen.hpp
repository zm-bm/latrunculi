#ifndef LATRUNCULI_MOVEGEN_H
#define LATRUNCULI_MOVEGEN_H

#include <iostream>
#include <vector>

#include "bb.hpp"
#include "chess.hpp"
#include "magics.hpp"
#include "move.hpp"
#include "types.hpp"

enum GenType { EVASIONS, CAPTURES, QUIETS };

class MoveGenerator {
   public:
    MoveGenerator(Chess* chess);

    void generatePseudoLegalMoves();
    void generateCaptures();

    std::vector<Move> moves;

   private:
    Chess* chess;

    void generateEvasions();

    template <GenType>
    void generateMovesToTarget(U64);

    template <GenType, Color>
    void generatePawnMoves(U64, U64);

    template <GenType>
    void generateKingMoves(U64, U64);

    template <PieceType, Color>
    void generatePieceMoves(U64, U64);

    void generateCastling(U64, Color);

    template <PawnMove, Color>
    void addPawnMoves(U64);

    template <PawnMove, Color, GenType>
    void addPawnPromotions(U64);

    template <PawnMove, Color>
    void addEnPassants(U64, Square);

    bool canCastleOO(U64 occ, Color turn);
    bool canCastleOOO(U64 occ, Color turn);
};

template <PawnMove p, Color c>
inline void MoveGenerator::addPawnMoves(U64 bitboard) {
    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square to = BB::advanced<c>(bitboard);
        bitboard &= BB::clear(to);

        // Reverse the move to get the origin square and append move
        Square from = pawnMove<c, p, false>(to);
        moves.push_back(Move(from, to));
    }
};

template <PawnMove p, Color c, GenType g>
inline void MoveGenerator::addPawnPromotions(U64 bitboard) {
    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square to = BB::advanced<c>(bitboard);
        bitboard &= BB::clear(to);

        // Reverse the move to get the origin square and append moves
        Square from = pawnMove<c, p, false>(to);

        if (g != QUIETS) {
            moves.push_back(Move(from, to, PROMOTION, QUEEN));
        }

        if (g != CAPTURES) {
            moves.push_back(Move(from, to, PROMOTION, ROOK));
            moves.push_back(Move(from, to, PROMOTION, BISHOP));
            moves.push_back(Move(from, to, PROMOTION, KNIGHT));
        }
    }
}

template <PawnMove p, Color c>
inline void MoveGenerator::addEnPassants(U64 pawns, Square enpassant) {
    U64 bitboard = BB::pawnMoves<p, c>(pawns) & BB::set(enpassant);

    if (bitboard) {
        Square from = pawnMove<c, p, false>(enpassant);
        moves.push_back(Move(from, enpassant, ENPASSANT));
    }
}

inline bool MoveGenerator::canCastleOO(U64 occ, Color turn) {
    return (chess->state.at(chess->ply).canCastleOO(turn)  // castling rights
            && !(occ & BB::CastlePathOO[turn])             // castle path unoccupied/attacked
            && !chess->board.isBitboardAttacked(BB::KingCastlePathOO[turn], ~turn));
}

inline bool MoveGenerator::canCastleOOO(U64 occ, Color turn) {
    return (chess->state.at(chess->ply).canCastleOOO(turn)  // castling rights
            && !(occ & BB::CastlePathOOO[turn])             // castle path unoccupied/attacked
            && !chess->board.isBitboardAttacked(BB::KingCastlePathOOO[turn], ~turn));
}

#endif
