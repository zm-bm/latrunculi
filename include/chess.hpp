#ifndef LATRUNCULI_CHESS_H
#define LATRUNCULI_CHESS_H

#include <vector>

#include "board.hpp"
#include "state.hpp"

class Chess {
   private:
    std::vector<State> state = {State()};
    Board board = Board(G::STARTFEN);
    Color turn = WHITE;
    U32 ply = 0;
    U32 moveCounter = 0;

   public:
    explicit Chess(const std::string&);

    void make(Move);
    void unmake();
    void makeNull();
    void unmmakeNull();
    template <bool>
    void makeCastle(Square, Square, Color);

    bool isLegalMove(Move) const;
    bool isCheckingMove(Move) const;

    template <bool>
    void addPiece(Square, Color, PieceRole);
    template <bool>
    void removePiece(Square, Color, PieceRole);
    template <bool>
    void movePiece(Square, Square, Color, PieceRole);

    U64 getCheckingPieces() const;
    Square getEnPassant() const;
    U8 getHmClock() const;
    U64 getKey() const;
    bool isCheck() const;
    bool isDoubleCheck() const;
    bool canCastle(Color c) const;
    bool canCastleOO(Color c) const;
    bool canCastleOOO(Color c) const;
    void disableCastle(Color c);
    void disableCastle(Color c, Square sq);
    void disableCastleOO(Color c);
    void disableCastleOOO(Color c);
    void setEnPassant(Square sq);
    void updateCapturedPieces(Square sq, Color c, PieceRole p);
    void handlePawnMoves(Square from, Square to, MoveType movetype, Move mv);
    int calculatePhase() const;
    U64 calculateKey() const;
    void updateState(bool);

    template <bool>
    int eval() const;

    std::string toFEN() const;
    std::string DebugString() const;
    friend std::ostream& operator<<(std::ostream& os, const Chess& chess);

    friend class MoveGen;
};

template <bool forward>
inline void Chess::addPiece(Square sq, Color c, PieceRole p) {
    board.addPiece(sq, c, p);

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][sq];
    }
}

template <bool forward>
inline void Chess::removePiece(Square sq, Color c, PieceRole p) {
    board.removePiece(sq, c, p);

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][sq];
    }
}

template <bool forward>
inline void Chess::movePiece(Square from, Square to, Color c, PieceRole p) {
    board.movePiece(from, to, c, p);

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][from] ^ Zobrist::psq[c][p][to];
    }
}

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
    // Check if the specified color can castle
    return (c ? state[ply].castle & WHITE_CASTLE
              : state[ply].castle & BLACK_CASTLE);
}

inline bool Chess::canCastleOO(Color c) const {
    // Check if the specified color can castle kingside (OO)
    return (c ? state[ply].castle & WHITE_OO : state[ply].castle & BLACK_OO);
}

inline bool Chess::canCastleOOO(Color c) const {
    // Check if the specified color can castle queenside (OOO)
    return (c ? state[ply].castle & WHITE_OOO : state[ply].castle & BLACK_OOO);
}

inline void Chess::disableCastle(Color c) {
    // Disable castling for the specified color
    if (canCastleOO(c)) state[ply].zkey ^= Zobrist::castle[c][KINGSIDE];
    if (canCastleOOO(c)) state[ply].zkey ^= Zobrist::castle[c][QUEENSIDE];

    // Update the castle rights based on the color
    state[ply].castle &= c ? BLACK_CASTLE : WHITE_CASTLE;
}

inline void Chess::disableCastle(Color c, Square sq) {
    // Disable casting for the specified color and side
    if (sq == G::RookOriginOO[c] && canCastleOO(c)) {
        disableCastleOO(c);
    } else if (sq == G::RookOriginOOO[c] && canCastleOOO(c)) {
        disableCastleOOO(c);
    }
}

inline void Chess::disableCastleOO(Color c) {
    // Disable kingside castling for the specified color
    state[ply].zkey ^= Zobrist::castle[c][KINGSIDE];
    state[ply].castle &= c ? CastleRights(0x07) : CastleRights(0x0D);
}

inline void Chess::disableCastleOOO(Color c) {
    // Disable queenside castling for the specified color
    state[ply].zkey ^= Zobrist::castle[c][QUEENSIDE];
    state[ply].castle &= c ? CastleRights(0x0B) : CastleRights(0x0E);
}

inline void Chess::setEnPassant(Square sq) {
    // Set the en passant target square and update the hash key
    state.at(ply).enPassantSq = sq;
    state.at(ply).zkey ^= Zobrist::ep[Types::getFile(sq)];
}

inline void Chess::updateCapturedPieces(Square sq, Color c, PieceRole p) {
    // Reset half move clock for capture
    state[ply].hmClock = 0;
    removePiece<true>(sq, c, p);

    // Disable castle rights if captured piece is rook
    if (canCastle(c) && p == ROOK) {
        disableCastle(c, sq);
    }
}

inline void Chess::handlePawnMoves(Square from, Square to, MoveType movetype,
                                   Move mv) {
    state[ply].hmClock = 0;  // Reset half move clock

    auto doubleMove = static_cast<U8>(PawnMove::DOUBLE);
    if ((from - to) == doubleMove || (to - from) == doubleMove) {
        setEnPassant(Types::pawnMove<PawnMove::PUSH, false>(to, turn));
    } else if (movetype == PROMOTION) {
        removePiece<true>(to, turn, PAWN);
        addPiece<true>(to, turn, mv.promPiece());
    }
}

inline int Chess::calculatePhase() const {
    return (
        PAWNSCORE * board.count<PAWN>() + KNIGHTSCORE * board.count<KNIGHT>() +
        BISHOPSCORE * board.count<BISHOP>() + ROOKSCORE * board.count<ROOK>() +
        QUEENSCORE * board.count<QUEEN>());
}

inline U64 Chess::calculateKey() const {
    U64 zkey = 0x0;
    auto sq = getEnPassant();

    for (auto sq = A1; sq != INVALID; sq++) {
        auto piece = board.getPiece(sq);

        if (piece != NO_PIECE) {
            auto c = Types::getPieceColor(piece);
            auto p = Types::getPieceRole(piece);
            zkey ^= Zobrist::psq[c][p][sq];
        }
    }

    if (turn == BLACK) zkey ^= Zobrist::stm;
    if (sq != INVALID) zkey ^= Zobrist::ep[Types::getFile(sq)];
    if (canCastleOO(WHITE)) zkey ^= Zobrist::castle[WHITE][KINGSIDE];
    if (canCastleOOO(WHITE)) zkey ^= Zobrist::castle[WHITE][QUEENSIDE];
    if (canCastleOO(BLACK)) zkey ^= Zobrist::castle[BLACK][KINGSIDE];
    if (canCastleOOO(BLACK)) zkey ^= Zobrist::castle[BLACK][QUEENSIDE];

    return zkey;
}

// After making a move, update the incrementally updated state helper variables
inline void Chess::updateState(bool checkingMove = true) {
    if (checkingMove) {
        state[ply].checkingPieces = board.calculateCheckingPieces(turn);
    } else {
        state[ply].checkingPieces = 0;
    }

    Color enemy = ~turn;
    Square king = board.getKingSq(enemy);
    U64 occ = board.occupancy();

    state[ply].pinnedPieces = board.calculatePinnedPieces(turn);
    state[ply].discoveredCheckers = board.calculateDiscoveredCheckers(turn);
    state[ply].checkingSquares[PAWN] = BB::attacksByPawns(BB::set(king), enemy);
    state[ply].checkingSquares[KNIGHT] = BB::movesByPiece<KNIGHT>(king, occ);
    state[ply].checkingSquares[BISHOP] = BB::movesByPiece<BISHOP>(king, occ);
    state[ply].checkingSquares[ROOK] = BB::movesByPiece<ROOK>(king, occ);
    state[ply].checkingSquares[QUEEN] =
        state[ply].checkingSquares[BISHOP] | state[ply].checkingSquares[ROOK];
}

#endif