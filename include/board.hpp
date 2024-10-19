#ifndef LATRUNCULI_BOARD_H
#define LATRUNCULI_BOARD_H

#include <iostream>
#include <vector>

#include "bb.hpp"
#include "types.hpp"
#include "zobrist.hpp"

struct Board {
    U64 pieces[2][7] = {0};
    Piece squares[64] = {NO_PIECE};
    Square kingSq[2] = {E1, E8};
    U8 pieceCount[2][7] = {0};

    explicit Board(const std::string&);
    friend std::ostream& operator<<(std::ostream& os, const Board& b);

    // int calculateMobilityScore(const int, const int) const;
    // template <PieceRole>
    // int calculateMobilityScore(const int, const int) const;
    // template <PieceRole p>
    // int calculateMobility(U64 bitboard, U64 targets, U64 occ) const;

    void addPiece(Square, Color, PieceRole);
    void removePiece(Square, Color, PieceRole);
    void movePiece(Square, Square, Color, PieceRole);

    template <PieceRole p>
    U64 getPieces(Color c) const;
    U64 occupancy() const;
    U64 diagonalSliders(Color c) const;
    U64 straightSliders(Color c) const;
    U64 attacksTo(Square, Color) const;
    U64 attacksTo(Square, Color, U64) const;
    U64 calculateCheckBlockers(Color, Color) const;
    U64 calculateDiscoveredCheckers(Color c) const;
    U64 calculatePinnedPieces(Color c) const;
    U64 calculateCheckingPieces(Color c) const;

    template <PieceRole p>
    int count(Color c) const;
    template <PieceRole p>
    int count() const;
    int getPieceCount(Color c) const;

    Piece getPiece(Square sq) const;
    PieceRole getPieceRole(Square sq) const;
    Square getKingSq(Color c) const;

    bool isBitboardAttacked(U64, Color) const;
};

inline void Board::addPiece(const Square sq, const Color c, const PieceRole p) {
    // Toggle bitboards and add to square centric board
    pieces[c][ALL_PIECE_ROLES] ^= BB::set(sq);
    pieces[c][p] ^= BB::set(sq);
    squares[sq] = Defs::makePiece(c, p);
    pieceCount[c][p]++;
}

inline void Board::removePiece(const Square sq, const Color c,
                               const PieceRole p) {
    // Toggle bitboards
    pieces[c][ALL_PIECE_ROLES] ^= BB::set(sq);
    pieces[c][p] ^= BB::set(sq);
    // squares[sq] = NO_PIECE;
    pieceCount[c][p]--;
}

inline void Board::movePiece(const Square from, const Square to, const Color c,
                             const PieceRole p) {
    // Toggle bitboards and add to square centric board
    U64 mask = BB::set(from) | BB::set(to);
    pieces[c][ALL_PIECE_ROLES] ^= mask;
    pieces[c][p] ^= mask;
    squares[from] = NO_PIECE;
    squares[to] = Defs::makePiece(c, p);
}

template <PieceRole p>
inline U64 Board::getPieces(Color c) const {
    // Return the bitboard of pieces of a specific role for the given color
    return pieces[c][p];
}

inline U64 Board::occupancy() const {
    // Return the combined bitboard of all pieces on the board
    return pieces[WHITE][ALL_PIECE_ROLES] | pieces[BLACK][ALL_PIECE_ROLES];
}

inline U64 Board::diagonalSliders(Color c) const {
    // Return the bitboard of diagonal sliding pieces (Bishops and Queens) for
    // the given color
    return pieces[c][BISHOP] | pieces[c][QUEEN];
}

inline U64 Board::straightSliders(Color c) const {
    // Return the bitboard of straight sliding pieces (Rooks and Queens) for the
    // given color
    return pieces[c][ROOK] | pieces[c][QUEEN];
}

inline U64 Board::attacksTo(Square sq, Color c) const {
    // Returns a bitboard of pieces of color c which attacks a square
    return attacksTo(sq, c, occupancy());
}

inline U64 Board::attacksTo(Square sq, Color c, U64 occ) const {
    // Returns a bitboard of pieces of color c which attacks a square
    U64 piece = BB::set(sq);

    return (getPieces<PAWN>(c) & BB::attacksByPawns(piece, ~c)) |
           (getPieces<KNIGHT>(c) & BB::movesByPiece<KNIGHT>(sq, occ)) |
           (getPieces<KING>(c) & BB::movesByPiece<KING>(sq, occ)) |
           (diagonalSliders(c) & BB::movesByPiece<BISHOP>(sq, occ)) |
           (straightSliders(c) & BB::movesByPiece<ROOK>(sq, occ));
}

inline U64 Board::calculateCheckBlockers(Color c, Color kingC) const {
    // Determine pieces of color c, which block the color kingC from attack by
    // the enemy
    Color enemy = ~kingC;
    Square king = getKingSq(kingC);

    // Determine which enemy sliders could check the kingC's king
    U64 blockers = 0,
        pinners = (BB::movesByPiece<BISHOP>(king) & diagonalSliders(enemy)) |
                  (BB::movesByPiece<ROOK>(king) & straightSliders(enemy));

    while (pinners) {
        // For each potential pinning piece
        Square pinner = BB::lsb(pinners);
        pinners &= BB::clear(pinner);

        // Check if only one piece separates the slider and the king
        U64 piecesInBetween = occupancy() & BB::bitsBtwn(king, pinner);
        if (!BB::moreThanOneSet(piecesInBetween)) {
            blockers |= piecesInBetween & getPieces<ALL_PIECE_ROLES>(c);
        }
    }

    return blockers;
}

inline U64 Board::calculateDiscoveredCheckers(Color c) const {
    // Calculate and return the bitboard of pieces that can give discovered
    // check
    return calculateCheckBlockers(c, ~c);
}

inline U64 Board::calculatePinnedPieces(Color c) const {
    // Calculate and return the bitboard of pieces that are pinned relative to
    // the king
    return calculateCheckBlockers(c, c);
}

inline U64 Board::calculateCheckingPieces(Color c) const {
    // Calculate and return the bitboard of pieces attacking the king
    return attacksTo(getKingSq(c), ~c);
}

template <PieceRole p>
inline int Board::count(Color c) const {
    // Return the count of pieces of a specific role for the given color
    return pieceCount[c][p];
}

template <PieceRole p>
inline int Board::count() const {
    // Return the total count of pieces of a specific role for both colors
    return count<p>(WHITE) + count<p>(BLACK);
}

inline int Board::getPieceCount(Color c) const {
    // Return the total count of all major pieces (Knights, Bishops, Rooks,
    // Queens) for the given color
    return count<KNIGHT>(c) + count<BISHOP>(c) + count<ROOK>(c) +
           count<QUEEN>(c);
}

inline Piece Board::getPiece(Square sq) const {
    // Return the piece located at a specific square
    return squares[sq];
}

inline PieceRole Board::getPieceRole(Square sq) const {
    // Return the role of the piece located at a specific square
    return Defs::getPieceRole(squares[sq]);
}

inline Square Board::getKingSq(Color c) const {
    // Return the square of the king for the given color
    return kingSq[c];
}

inline bool Board::isBitboardAttacked(U64 bitboard, Color c) const {
    // Determine if any set square of a bitboard is attacked by color c
    while (bitboard) {
        Square sq = BB::lsb(bitboard);
        bitboard &= BB::clear(sq);

        if (attacksTo(sq, c)) return true;
    }

    return false;
}

#endif
