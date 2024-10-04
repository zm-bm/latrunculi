#ifndef LATRUNCULI_BOARD_H
#define LATRUNCULI_BOARD_H

#include <iostream>
#include <vector>

#include "bb.hpp"
#include "eval.hpp"
#include "types.hpp"
#include "zobrist.hpp"
#include "movegen.hpp"

struct Board {
    U64 pieces[2][7];
    Piece squares[64];
    Square kingSq[2];
    U8 pieceCount[2][7];
    I32 openingScore;
    I32 endgameScore;
    I32 materialScore;

    explicit Board(const std::string&);
    void loadFEN(const std::string&);

    U64 getCheckBlockers(Color, Color) const;

    int calculateMobilityScore(const int, const int) const;

    template <PieceRole>
    int calculateMobilityScore(const int, const int) const;

    template <PieceRole p>
    int calculateMobility(U64 bitboard, U64 targets, U64 occ) const;

    friend std::ostream& operator<<(std::ostream& os, const Board& b);

    template <bool>
    inline void addPiece(Square, Color, PieceRole);

    template <bool>
    inline void removePiece(Square, Color, PieceRole);

    template <bool>
    inline void movePiece(Square, Square, Color, PieceRole);

    inline U64 attacksTo(Square, Color) const;
    inline U64 attacksTo(Square, Color, U64) const;
    inline bool isBitboardAttacked(U64, Color) const;

    template <PieceRole p>
    inline U64 getPieces(Color c) const {
        // Return the bitboard of pieces of a specific role for the given color
        return pieces[c][p];
    }

    inline U64 occupancy() const {
        // Return the combined bitboard of all pieces on the board
        return pieces[WHITE][ALL_PIECE_ROLES] | pieces[BLACK][ALL_PIECE_ROLES];
    }

    inline U64 diagonalSliders(Color c) const {
        // Return the bitboard of diagonal sliding pieces (Bishops and Queens) for the given color
        return pieces[c][BISHOP] | pieces[c][QUEEN];
    }

    inline U64 straightSliders(Color c) const {
        // Return the bitboard of straight sliding pieces (Rooks and Queens) for the given color
        return pieces[c][ROOK] | pieces[c][QUEEN];
    }

    inline U64 calculateCheckingPieces(Color c) const {
        // Calculate and return the bitboard of pieces attacking the enemy king
        return attacksTo(getKingSq(c), ~c);
    }

    inline U64 calculateDiscoveredCheckers(Color c) const {
        // Calculate and return the bitboard of pieces that can give discovered check
        return getCheckBlockers(c, ~c);
    }

    inline U64 calculatePinnedPieces(Color c) const {
        // Calculate and return the bitboard of pieces that are pinned relative to the king
        return getCheckBlockers(c, c);
    }

    template <PieceRole pt>
    inline U8 count(Color c) const {
        // Return the count of pieces of a specific role for the given color
        return pieceCount[c][pt];
    }

    template <PieceRole pt>
    inline U8 count() const {
        // Return the total count of pieces of a specific role for both colors
        return count<pt>(WHITE) + count<pt>(BLACK);
    }

    inline Piece getPiece(Square sq) const {
        // Return the piece located at a specific square
        return squares[sq];
    }

    inline PieceRole getPieceRole(Square sq) const {
        // Return the role of the piece located at a specific square
        return Types::getPieceRole(squares[sq]);
    }
    
    inline Square getKingSq(Color c) const {
        // Return the square of the king for the given color
        return kingSq[c];
    }
    
    inline int getPieceCount(Color c) const {
        // Return the total count of all major pieces (Knights, Bishops, Rooks, Queens) for the given color
        return count<KNIGHT>(c) + count<BISHOP>(c) + count<ROOK>(c) + count<QUEEN>(c);
    }
};

template <bool forward>
inline void Board::addPiece(const Square sq, const Color c,
                            const PieceRole pt) {
    // Toggle bitboards and add to square centric board
    pieces[c][ALL_PIECE_ROLES] ^= BB::set(sq);
    pieces[c][pt] ^= BB::set(sq);
    squares[sq] = Types::makePiece(c, pt);

    // Update evaluation helpers
    pieceCount[c][pt]++;
    materialScore += Eval::PieceValues[pt - 1][c];
    openingScore += Eval::psqv(c, pt, OPENING, sq);
    endgameScore += Eval::psqv(c, pt, ENDGAME, sq);

    // if (forward) state.at(ply).zkey ^= Zobrist::psq[c][pt - 1][sq];
}

template <bool forward>
inline void Board::removePiece(const Square sq, const Color c,
                               const PieceRole pt) {
    // Toggle bitboards
    pieces[c][ALL_PIECE_ROLES] ^= BB::set(sq);
    pieces[c][pt] ^= BB::set(sq);

    // Update evaluation helpers
    pieceCount[c][pt]--;
    materialScore -= Eval::PieceValues[pt - 1][c];
    openingScore -= Eval::psqv(c, pt, OPENING, sq);
    endgameScore -= Eval::psqv(c, pt, ENDGAME, sq);

    // if (forward) state.at(ply).zkey ^= Zobrist::psq[c][pt - 1][sq];
}

template <bool forward>
inline void Board::movePiece(const Square from, const Square to, const Color c,
                             const PieceRole pt) {
    // Toggle bitboards and add to square centric board
    U64 mask = BB::set(from) | BB::set(to);
    pieces[c][ALL_PIECE_ROLES] ^= mask;
    pieces[c][pt] ^= mask;
    squares[from] = NO_PIECE;
    squares[to] = Types::makePiece(c, pt);

    // Update evaluation helpers
    openingScore +=
        Eval::psqv(c, pt, OPENING, to) - Eval::psqv(c, pt, OPENING, from);
    endgameScore +=
        Eval::psqv(c, pt, ENDGAME, to) - Eval::psqv(c, pt, ENDGAME, from);

    // if (forward)
    //     state.at(ply).zkey ^=
    //         Zobrist::psq[c][pt - 1][from] ^ Zobrist::psq[c][pt - 1][to];
}

// Returns a bitboard of pieces of color c which attacks a square
inline U64 Board::attacksTo(Square sq, Color c) const {
    return attacksTo(sq, c, occupancy());
}

// Returns a bitboard of pieces of color c which attacks a square
inline U64 Board::attacksTo(Square sq, Color c, U64 occ) const {
    U64 piece = BB::set(sq);

    return (getPieces<PAWN>(c) & BB::attacksByPawns(piece, ~c)) |
           (getPieces<KNIGHT>(c) & BB::movesByPiece<KNIGHT>(sq, occ)) |
           (getPieces<KING>(c) & BB::movesByPiece<KING>(sq, occ)) |
           (diagonalSliders(c) & BB::movesByPiece<BISHOP>(sq, occ)) |
           (straightSliders(c) & BB::movesByPiece<ROOK>(sq, occ));
}

// Determine if any set square of a bitboard is attacked by color c
inline bool Board::isBitboardAttacked(U64 bitboard, Color c) const {
    while (bitboard) {
        Square sq = BB::lsb(bitboard);
        bitboard &= BB::clear(sq);

        if (attacksTo(sq, c)) return true;
    }

    return false;
}

#endif
