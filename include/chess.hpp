#ifndef LATRUNCULI_CHESS_H
#define LATRUNCULI_CHESS_H

#include <vector>

#include "board.hpp"
#include "state.hpp"

class Chess {

private:
    std::vector<State> state;
    Board board;
    Color stm;
    U32 ply;
    U32 fullMoveCounter;

public:
    explicit Chess(const std::string&);

    void make(Move);
    void unmake();
    void makeNull();
    void unmmakeNull();
    template <bool>
    void makeCastle(Square, Square, Color);

    template <bool>
    int eval() const;

    bool isLegalMove(Move) const;
    bool isCheckingMove(Move) const;
    U64 calculateKey() const;

    inline U64 getCheckingPieces() const;
    inline Square getEnPassant() const;
    inline U8 getHmClock() const;
    inline U64 getKey() const;
    inline bool isCheck() const;
    inline bool isDoubleCheck() const;
    inline bool canCastle(Color c) const;
    inline bool canCastleOO(Color c) const;
    inline bool canCastleOOO(Color c) const; 
    inline void disableCastle(Color c);
    inline void disableCastleOO(Color c); 
    inline void disableCastleOOO(Color c); 
    inline void setEnPassant(Square sq);
    inline void updateState(bool inCheck);

    std::string toFEN() const;
    void loadFEN(const std::string&);
    std::string DebugString() const;
    friend std::ostream& operator<<(std::ostream& os, const Chess& chess);
    friend class MoveGen::Generator;
};

inline U64 Chess::getCheckingPieces() const {
    // Return the bitboard of pieces that are checking the current player's king
    return state.at(ply).checkingPieces;
}

inline Square Chess::getEnPassant() const {
    // Return the en passant target square if set
    return state.at(ply).enPassantSq;
}

inline U8 Chess::getHmClock() const {
    // Return the half-move clock from the current state
    return state.at(ply).hmClock;
}

inline U64 Chess::getKey() const {
    // Return the Zobrist key for the current board state
    return state.at(ply).zkey;
}

inline bool Chess::isCheck() const {
    // Return whether the current player's king is in check
    return getCheckingPieces();
}

inline bool Chess::isDoubleCheck() const {
    // Return whether the current player's king is in double check
    return BB::moreThanOneSet(getCheckingPieces());
}

inline bool Chess::canCastle(Color c) const {
    if (c)
        return state[ply].castle & WHITE_CASTLE;
    else
        return state[ply].castle & BLACK_CASTLE;
}

inline bool Chess::canCastleOO(Color c) const {
    if (c)
        return state[ply].castle & WHITE_OO;
    else
        return state[ply].castle & BLACK_OO;
}

inline bool Chess::canCastleOOO(Color c) const {
    if (c)
        return state[ply].castle & WHITE_OOO;
    else
        return state[ply].castle & BLACK_OOO;
}

inline void Chess::disableCastle(Color c) {
    if (canCastleOO(c)) state[ply].zkey ^= Zobrist::castle[c][KINGSIDE];
    if (canCastleOOO(c)) state[ply].zkey ^= Zobrist::castle[c][QUEENSIDE];

    if (c)
        state[ply].castle &= BLACK_CASTLE;
    else
        state[ply].castle &= WHITE_CASTLE;
}

inline void Chess::disableCastleOO(Color c) {
    state[ply].zkey ^= Zobrist::castle[c][KINGSIDE];

    if (c)
        state[ply].castle &= CastleRights(0x07);
    else
        state[ply].castle &= CastleRights(0x0D);
}

inline void Chess::disableCastleOOO(Color c) {
    state[ply].zkey ^= Zobrist::castle[c][QUEENSIDE];

    if (c)
        state[ply].castle &= CastleRights(0x0B);
    else
        state[ply].castle &= CastleRights(0x0E);
}

inline void Chess::setEnPassant(Square sq) {
    state.at(ply).enPassantSq = sq;
    state.at(ply).zkey ^= Zobrist::ep[Types::getFile(sq)];
}

// After making a move, update the incrementally updated state helper variables
inline void Chess::updateState(bool inCheck = true) {
    if (inCheck)
        state[ply].checkingPieces = board.calculateCheckingPieces(stm);
    else
        state[ply].checkingPieces = 0;

    state[ply].pinnedPieces = board.calculatePinnedPieces(stm);
    state[ply].discoveredCheckers = board.calculateDiscoveredCheckers(stm);

    Color enemy = ~stm;
    Square king = board.getKingSq(enemy);
    U64 occ = board.occupancy();

    state[ply].checkingSquares[PAWN] =
        BB::attacksByPawns(BB::set(king), enemy);
    state[ply].checkingSquares[KNIGHT] =
        BB::movesByPiece<KNIGHT>(king, occ);
    state[ply].checkingSquares[BISHOP] =
        BB::movesByPiece<BISHOP>(king, occ);
    state[ply].checkingSquares[ROOK] = BB::movesByPiece<ROOK>(king, occ);
    state[ply].checkingSquares[QUEEN] =
        state[ply].checkingSquares[BISHOP] | state[ply].checkingSquares[ROOK];
}

#endif