#ifndef LATRUNCULI_BOARD_H
#define LATRUNCULI_BOARD_H

#include <iostream>
#include <vector>
#include "types.hpp"
#include "bitboard.hpp"
#include "state.hpp"
#include "movegen.hpp"
#include "eval.hpp"
#include "zobrist.hpp"

class Board {
    private:
        // Board state history
        std::vector<State> state;

        // Square and piece centric board representations
        BB pieces[2][7];
        Piece squares[64];

        // Convenience variables
        Square kingSq[2];
        U8 pieceCount[2][7];

        // Counters
        U32 ply;
        U32 fullMoveCounter;

        // Evaluation
        I32 openingScore;
        I32 endgameScore;
        I32 materialScore;

        // Side to move
        Color stm;

    public:
        // Board.cpp
        explicit Board(const std::string&);

        bool isLegalMove(Move) const;
        bool isCheckingMove(Move) const;
        BB getCheckBlockers(Color, Color) const;

                                    int calculateMobilityScore(const int, const int) const;
        template<PieceType>         int calculateMobilityScore(const int, const int) const;
                                    int calculatePieceScore() const;
        template<PieceType, Color>  int calculatePieceScore() const;

        std::string toFEN() const;
        std::string DebugString() const;
        friend std::ostream& operator<<(std::ostream& os, const Board& b);
        void loadFEN(const std::string&);
        void loadFENPieces(const std::string&);
        void loadFENEnPassant(const std::string&);
        void loadFENCastle(const std::string&);
        U64 calculateKey() const;

        // make.cpp
        void make(Move);
        void unmake();
        void makeNull();
        void unmmakeNull();
        void updateState(bool=true);
        template<bool> void makeCastle(Square, Square, Color);

        // eval.cpp
        template<bool> int eval() const;
        
        // Inline functions
        template<bool> void addPiece(Square, Color, PieceType);
        template<bool> void removePiece(Square, Color, PieceType);
        template<bool> void movePiece(Square, Square, Color, PieceType);

        template<PieceType> U8 count(Color c) const;
        template<PieceType> U8 count() const;
        int calculatePhase() const;

        BB attacksTo(Square, Color) const;
        BB attacksTo(Square, Color, BB) const;
        bool isBitboardAttacked(BB, Color) const;

        bool canCastle(Color) const;
        bool canCastleOO(Color) const;
        bool canCastleOOO(Color) const;
        void disableCastle(Color);
        void disableCastleOO(Color);
        void disableCastleOOO(Color);
        void setEnPassant(Square sq);

        template<PieceType p>
        inline BB getPieces(Color c) const { return pieces[c][p]; }

        inline BB occupancy() const { return pieces[WHITE][ALL] | pieces[BLACK][ALL]; }
        inline BB diagonalSliders(Color c) const {return pieces[c][BISHOP] | pieces[c][QUEEN]; }
        inline BB straightSliders(Color c) const {return pieces[c][ROOK] | pieces[c][QUEEN]; }
        inline BB getCheckingPieces() const { return state.at(ply).checkingPieces; }
        inline BB calculateCheckingPieces(Color c) const { return attacksTo(getKingSq(c), ~c); }
        inline BB calculateDiscoveredCheckers(Color c) const { return getCheckBlockers(c, ~c); }
        inline BB calculatePinnedPieces(Color c) const { return getCheckBlockers(c, c); }

        inline Piece getPiece(Square sq) const { return squares[sq]; }
        inline PieceType getPieceType(Square sq) const { return Types::getPieceType(squares[sq]); }
        inline Color sideToMove() const { return stm; }
        inline Square getKingSq(Color c) const { return kingSq[c]; }
        inline Square getEnPassant() const { return state.at(ply).enPassantSq; }
        inline U8 getHmClock() const { return state.at(ply).hmClock; }
        inline U64 getKey() const { return state.at(ply).zkey; }
        inline bool isCheck() const { return getCheckingPieces(); }
        inline bool isDoubleCheck() const { return getCheckingPieces().moreThanOneSet(); }
        inline int getPieceCount(Color c) const {
            return count<KNIGHT>(c) + count<BISHOP>(c) + count<ROOK>(c) + count<QUEEN>(c);
        }

        friend class MoveGen::Generator;
        friend class Search;
};

template<bool forward>
inline void Board::addPiece(const Square sq, const Color c, const PieceType pt)
{
    // Toggle bitboards and add to square centric board
    pieces[c][ALL].toggle(sq);
    pieces[c][pt].toggle(sq);
    squares[sq] = Types::makePiece(c, pt);

    // Update evaluation helpers
    pieceCount[c][pt]++;
    materialScore += Eval::PieceValues[pt-1][c];
    openingScore += Eval::psqv(c, pt, OPENING, sq);
    endgameScore += Eval::psqv(c, pt, ENDGAME, sq);

    if (forward)
        state.at(ply).zkey ^= Zobrist::psq[c][pt-1][sq];
}

template<bool forward>
inline void Board::removePiece(const Square sq, const Color c, const PieceType pt)
{
    // Toggle bitboards
    pieces[c][ALL].toggle(sq);
    pieces[c][pt].toggle(sq);

    // Update evaluation helpers
    pieceCount[c][pt]--;
    materialScore -= Eval::PieceValues[pt-1][c];
    openingScore -= Eval::psqv(c, pt, OPENING, sq);
    endgameScore -= Eval::psqv(c, pt, ENDGAME, sq);

    if (forward)
        state.at(ply).zkey ^= Zobrist::psq[c][pt-1][sq];
}

template<bool forward>
inline void Board::movePiece(const Square from, const Square to, const Color c, const PieceType pt)
{
    // Toggle bitboards and add to square centric board
    BB mask = G::BITSET[from] | G::BITSET[to];
    pieces[c][ALL].toggle(mask);
    pieces[c][pt].toggle(mask);
    squares[from] = EMPTY;
    squares[to] =  Types::makePiece(c, pt);

    // Update evaluation helpers
    openingScore += Eval::psqv(c, pt, OPENING, to) - Eval::psqv(c, pt, OPENING, from);
    endgameScore += Eval::psqv(c, pt, ENDGAME, to) - Eval::psqv(c, pt, ENDGAME, from);

    if (forward)
        state.at(ply).zkey ^= Zobrist::psq[c][pt-1][from] ^ Zobrist::psq[c][pt-1][to];
}

template<PieceType pt>
inline U8 Board::count(Color c) const
{
    return pieceCount[c][pt];
}

template<PieceType pt>
inline U8 Board::count() const
{
    return count<pt>(WHITE) + count<pt>(BLACK);
}

inline int Board::calculatePhase() const
{
    return PAWNSCORE * count<PAWN>()
         + KNIGHTSCORE * count<KNIGHT>()
         + BISHOPSCORE * count<BISHOP>()
         + ROOKSCORE * count<ROOK>()
         + QUEENSCORE * count<QUEEN>();
}

// Returns a bitboard of pieces of color c which attacks a square
inline BB Board::attacksTo(Square sq, Color c) const
{
    return attacksTo(sq, c, occupancy());
}

// Returns a bitboard of pieces of color c which attacks a square
inline BB Board::attacksTo(Square sq, Color c, BB occ) const
{
    BB piece = BB(G::BITSET[sq]);

    return (getPieces<PAWN>(c)   & MoveGen::attacksByPawns(piece, ~c))
         | (getPieces<KNIGHT>(c) & MoveGen::movesByPiece<KNIGHT>(sq, occ))
         | (getPieces<KING>(c)   & MoveGen::movesByPiece<KING>(sq, occ))
         | (diagonalSliders(c)   & MoveGen::movesByPiece<BISHOP>(sq, occ))
         | (straightSliders(c)   & MoveGen::movesByPiece<ROOK>(sq, occ));
}

// Determine if any set square of a bitboard is attacked by color c
inline bool Board::isBitboardAttacked(BB bitboard, Color c) const
{
    while (bitboard)
    {
        Square sq = bitboard.lsb();
        bitboard.clear(sq);

        if(attacksTo(sq, c))
            return true;
    }

    return false;
}

inline bool Board::canCastle(Color c) const
{
    if (c)
        return state[ply].castle & WHITE_CASTLE;
    else
        return state[ply].castle & BLACK_CASTLE;
}

inline bool Board::canCastleOO(Color c) const
{
    if (c)
        return state[ply].castle & WHITE_OO;
    else
        return state[ply].castle & BLACK_OO;
}

inline bool Board::canCastleOOO(Color c) const
{
    if (c)
        return state[ply].castle & WHITE_OOO;
    else
        return state[ply].castle & BLACK_OOO;
}

inline void Board::disableCastle(Color c)
{
    if (canCastleOO(c))
        state[ply].zkey ^= Zobrist::castle[c][KINGSIDE];
    if (canCastleOOO(c))
        state[ply].zkey ^= Zobrist::castle[c][QUEENSIDE];

    if (c)
        state[ply].castle &= BLACK_CASTLE;
    else
        state[ply].castle &= WHITE_CASTLE;
}

inline void Board::disableCastleOO(Color c)
{
    state[ply].zkey ^= Zobrist::castle[c][KINGSIDE];

    if (c)
        state[ply].castle &= CastleRights(0x07);
    else
        state[ply].castle &= CastleRights(0x0D);
}

inline void Board::disableCastleOOO(Color c)
{
    state[ply].zkey ^= Zobrist::castle[c][QUEENSIDE];

    if (c)
        state[ply].castle &= CastleRights(0x0B);
    else
        state[ply].castle &= CastleRights(0x0E);
}

inline void Board::setEnPassant(Square sq)
{
    state.at(ply).enPassantSq = sq; 
    state.at(ply).zkey ^= Zobrist::ep[Types::getFile(sq)];
}

// After making a move, update the incrementally updated state helper variables
inline void Board::updateState(bool inCheck)
{
    if (inCheck)
        state[ply].checkingPieces = calculateCheckingPieces(stm);
    else
        state[ply].checkingPieces = 0;

    state[ply].pinnedPieces = calculatePinnedPieces(stm);
    state[ply].discoveredCheckers = calculateDiscoveredCheckers(stm);

    Color enemy = ~stm;
    Square king = getKingSq(enemy);
    BB occ = occupancy();

    state[ply].checkingSquares[PAWN]   = MoveGen::attacksByPawns(G::BITSET[king], enemy);
    state[ply].checkingSquares[KNIGHT] = MoveGen::movesByPiece<KNIGHT>(king, occ);
    state[ply].checkingSquares[BISHOP] = MoveGen::movesByPiece<BISHOP>(king, occ);
    state[ply].checkingSquares[ROOK]   = MoveGen::movesByPiece<ROOK>(king, occ);
    state[ply].checkingSquares[QUEEN]  = state[ply].checkingSquares[BISHOP]
                                       | state[ply].checkingSquares[ROOK];
}

#endif
