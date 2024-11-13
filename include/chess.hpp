#ifndef LATRUNCULI_CHESS_H
#define LATRUNCULI_CHESS_H

#include <string>
#include <vector>

#include "board.hpp"
#include "constants.hpp"
#include "eval.hpp"
#include "state.hpp"
#include "zobrist.hpp"

class Chess {
   private:
    std::vector<State> state = {State()};
    Board board = Board(STARTFEN);
    Color turn = WHITE;
    U32 ply = 0;
    U32 moveCounter = 0;

    I32 openingScore = 0;
    I32 endgameScore = 0;
    I32 materialScore = 0;

   public:
    explicit Chess(const std::string&);

    void make(Move);
    void unmake();
    void makeNull();
    void unmmakeNull();
    template <bool>
    void makeCastle(Square, Square, Color);

    bool isPseudoLegalMoveLegal(Move) const;
    bool isCheckingMove(Move) const;

    template <bool>
    void addPiece(Square, Color, PieceType);
    template <bool>
    void removePiece(Square, Color, PieceType);
    template <bool>
    void movePiece(Square, Square, Color, PieceType);

    U64 getCheckingPieces() const;
    Square getEnPassant() const;
    U8 getHmClock() const;
    U64 getKey() const;
    bool isCheck() const;
    bool isDoubleCheck() const;
    void setEnPassant(Square sq);
    void updateCapturedPieces(Square sq, Color c, PieceType p);
    void handlePawnMoves(Square from, Square to, MoveType movetype, Move mv);
    U64 calculateKey() const;
    void updateState(bool);

    template <bool>
    int eval() const;

    template <bool>
    int eval_mg() const;
    template <bool>
    int eval_eg() const;

    int phase() const;
    int non_pawn_material(Color) const;

    std::string toFEN() const;
    std::string DebugString() const;
    friend std::ostream& operator<<(std::ostream& os, const Chess& chess);

    friend class MoveGenerator;
};

template <bool forward>
inline void Chess::addPiece(Square sq, Color c, PieceType p) {
    board.addPiece(sq, c, p);
    // materialScore += Eval::PieceValues[p][c];
    // openingScore += Eval::psqv(c, p, MIDGAME, sq);
    // endgameScore += Eval::psqv(c, p, ENDGAME, sq);

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][sq];
    }
}

template <bool forward>
inline void Chess::removePiece(Square sq, Color c, PieceType p) {
    board.removePiece(sq, c, p);
    // materialScore -= Eval::PieceValues[p][c];
    // openingScore -= Eval::psqv(c, p, MIDGAME, sq);
    // endgameScore -= Eval::psqv(c, p, ENDGAME, sq);

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][sq];
    }
}

template <bool forward>
inline void Chess::movePiece(Square from, Square to, Color c, PieceType p) {
    board.movePiece(from, to, c, p);
    // openingScore += Eval::psqv(c, p, MIDGAME, to) - Eval::psqv(c, p, MIDGAME, from);
    // endgameScore += Eval::psqv(c, p, ENDGAME, to) - Eval::psqv(c, p, ENDGAME, from);

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

inline void Chess::setEnPassant(Square sq) {
    // Set the en passant target square and update the hash key
    state.at(ply).enPassantSq = sq;
    state.at(ply).zkey ^= Zobrist::ep[Defs::fileFromSq(sq)];
}

inline void Chess::updateCapturedPieces(Square sq, Color c, PieceType p) {
    // Reset half move clock for capture
    state[ply].hmClock = 0;
    removePiece<true>(sq, c, p);

    // Disable castle rights if captured piece is rook
    if (state.at(ply).canCastle(c) && p == ROOK) {
        state.at(ply).disableCastle(c, sq);
    }
}

inline void Chess::handlePawnMoves(Square from, Square to, MoveType movetype, Move mv) {
    state[ply].hmClock = 0;  // Reset half move clock

    auto doubleMove = static_cast<U8>(PawnMove::DOUBLE);
    if ((from - to) == doubleMove || (to - from) == doubleMove) {
        setEnPassant(Defs::pawnMove<PawnMove::PUSH, false>(to, turn));
    } else if (movetype == PROMOTION) {
        removePiece<true>(to, turn, PAWN);
        addPiece<true>(to, turn, mv.promoPiece());
    }
}



inline U64 Chess::calculateKey() const {
    U64 zkey = 0x0;
    auto sq = getEnPassant();

    for (auto sq = A1; sq != INVALID; sq++) {
        auto piece = board.getPiece(sq);

        if (piece != NO_PIECE) {
            auto c = Defs::getPieceColor(piece);
            auto p = Defs::getPieceType(piece);
            zkey ^= Zobrist::psq[c][p][sq];
        }
    }

    if (turn == BLACK) zkey ^= Zobrist::stm;
    if (sq != INVALID) zkey ^= Zobrist::ep[Defs::fileFromSq(sq)];
    if (state.at(ply).canCastleOO(WHITE)) zkey ^= Zobrist::castle[WHITE][KINGSIDE];
    if (state.at(ply).canCastleOOO(WHITE)) zkey ^= Zobrist::castle[WHITE][QUEENSIDE];
    if (state.at(ply).canCastleOO(BLACK)) zkey ^= Zobrist::castle[BLACK][KINGSIDE];
    if (state.at(ply).canCastleOOO(BLACK)) zkey ^= Zobrist::castle[BLACK][QUEENSIDE];

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