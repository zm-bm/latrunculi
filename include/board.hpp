#pragma once

#include <iostream>
#include <vector>

#include "bb.hpp"
#include "fen.hpp"
#include "types.hpp"
#include "zobrist.hpp"

struct Board {
    U64 piecesBB[N_COLORS][N_PIECES] = {0};
    Piece squares[N_SQUARES] = {NO_PIECE};
    Square kingSquare[N_COLORS] = {E1, E8};
    U8 pieceCount[N_COLORS][N_PIECES] = {0};

    Board() = default;
    explicit Board(const std::string&);
    friend std::ostream& operator<<(std::ostream& os, const Board& b);

    // mutators
    void addPiece(Square, Color, PieceType);
    void removePiece(Square, Color, PieceType);
    void movePiece(Square, Square, Color, PieceType);

    // accessors
    template <PieceType p>
    int count(Color c) const;
    template <PieceType p>
    U64 pieces(Color c) const;
    Piece pieceOn(Square sq) const;
    PieceType pieceTypeOn(Square sq) const;
    Square kingSq(Color c) const;

    // movegen helpers
    U64 occupancy() const;
    U64 diagonalSliders(Color c) const;
    U64 straightSliders(Color c) const;
    U64 attacksTo(Square, Color) const;
    U64 attacksTo(Square, Color, U64) const;
    U64 calculateCheckBlockers(Color, Color) const;
    U64 calculateDiscoveredCheckers(Color c) const;
    U64 calculatePinnedPieces(Color c) const;
    U64 calculateCheckingPieces(Color c) const;
    bool isBitboardAttacked(U64, Color) const;
};

inline Board::Board(const std::string& fen) {
    FenParser parser(fen);

    auto pieces = parser.pieces;
    for (auto piece = pieces.begin(); piece != pieces.end(); ++piece) {
        addPiece(piece->square, piece->color, piece->role);

        if (piece->role == KING) {
            kingSquare[piece->color] = piece->square;
        }
    }
}

inline void Board::addPiece(const Square sq, const Color c, const PieceType p) {
    // Toggle bitboards and add to square centric board
    piecesBB[c][ALL_PIECES] ^= BB::set(sq);
    piecesBB[c][p] ^= BB::set(sq);
    squares[sq] = makePiece(c, p);
    pieceCount[c][p]++;
}

inline void Board::removePiece(const Square sq, const Color c, const PieceType p) {
    // Toggle bitboards
    piecesBB[c][ALL_PIECES] ^= BB::set(sq);
    piecesBB[c][p] ^= BB::set(sq);
    // squares[sq] = NO_PIECE;
    pieceCount[c][p]--;
}

inline void Board::movePiece(const Square from, const Square to, const Color c, const PieceType p) {
    // Toggle bitboards and add to square centric board
    U64 mask = BB::set(from) | BB::set(to);
    piecesBB[c][ALL_PIECES] ^= mask;
    piecesBB[c][p] ^= mask;
    squares[from] = NO_PIECE;
    squares[to] = makePiece(c, p);
}

template <PieceType p>
inline int Board::count(Color c) const {
    // Return the count of pieces of a specific type for the given color
    return pieceCount[c][p];
}

template <PieceType p>
inline U64 Board::pieces(Color c) const {
    // Return the bitboard of pieces of a specific role for the given color
    return piecesBB[c][p];
}

inline Piece Board::pieceOn(Square sq) const {
    // Return the piece located at a specific square
    return squares[sq];
}

inline PieceType Board::pieceTypeOn(Square sq) const {
    // Return the role of the piece located at a specific square
    return pieceTypeOf(squares[sq]);
}

inline Square Board::kingSq(Color c) const {
    // Return the square of the king for the given color
    return kingSquare[c];
}

inline U64 Board::occupancy() const {
    // Return the combined bitboard of all pieces on the board
    return piecesBB[WHITE][ALL_PIECES] | piecesBB[BLACK][ALL_PIECES];
}

inline U64 Board::diagonalSliders(Color c) const {
    // Return the bitboard of diagonal sliding pieces (Bishops and Queens) for
    // the given color
    return piecesBB[c][BISHOP] | piecesBB[c][QUEEN];
}

inline U64 Board::straightSliders(Color c) const {
    // Return the bitboard of straight sliding pieces (Rooks and Queens) for the
    // given color
    return piecesBB[c][ROOK] | piecesBB[c][QUEEN];
}

inline U64 Board::attacksTo(Square sq, Color c) const {
    // Returns a bitboard of pieces of color c which attacks a square
    return attacksTo(sq, c, occupancy());
}

inline U64 Board::attacksTo(Square sq, Color c, U64 occ) const {
    // Returns a bitboard of pieces of color c which attacks a square
    U64 piece = BB::set(sq);

    return (pieces<PAWN>(c) & BB::pawnAttacks(piece, ~c)) |
           (pieces<KNIGHT>(c) & BB::pieceMoves<KNIGHT>(sq, occ)) |
           (pieces<KING>(c) & BB::pieceMoves<KING>(sq, occ)) |
           (diagonalSliders(c) & BB::pieceMoves<BISHOP>(sq, occ)) |
           (straightSliders(c) & BB::pieceMoves<ROOK>(sq, occ));
}

inline U64 Board::calculateCheckBlockers(Color c, Color kingC) const {
    // Determine pieces of color c, which block the color kingC from attack by
    // the enemy
    Color enemy = ~kingC;
    Square king = kingSq(kingC);

    // Determine which enemy sliders could check the kingC's king
    U64 blockers = 0;
    U64 pinners = (BB::pieceMoves<BISHOP>(king) & diagonalSliders(enemy)) |
                  (BB::pieceMoves<ROOK>(king) & straightSliders(enemy));

    while (pinners) {
        // For each potential pinning piece
        Square pinner = BB::lsb(pinners);
        pinners &= BB::clear(pinner);

        // Check if only one piece separates the slider and the king
        U64 piecesInBetween = occupancy() & BB::betweenBB(king, pinner);
        if (!BB::hasMoreThanOne(piecesInBetween)) {
            blockers |= piecesInBetween & pieces<ALL_PIECES>(c);
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
    return attacksTo(kingSq(c), ~c);
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

inline std::ostream& operator<<(std::ostream& os, const Board& b) {
    for (Rank rank = RANK8; rank >= RANK1; rank--) {
        os << "   +---+---+---+---+---+---+---+---+\n";
        os << "   |";
        for (File file = FILE1; file <= FILE8; file++) {
            os << " " << b.pieceOn(makeSquare(file, rank)) << " |";
        }
        os << " " << rank << '\n';
    }

    os << "   +---+---+---+---+---+---+---+---+\n";
    os << "     a   b   c   d   e   f   g   h\n\n";

    return os;
}
