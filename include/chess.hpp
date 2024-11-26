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

    int mgMaterialScore = 0;
    int egMaterialScore = 0;
    int mgPieceSqScore = 0;
    int egPieceSqScore = 0;

   public:
    explicit Chess(const std::string&);

    template <bool>
    void addPiece(Square, Color, PieceType);
    template <bool>
    void removePiece(Square, Color, PieceType);
    template <bool>
    void movePiece(Square, Square, Color, PieceType);
    void updateState(bool);
    U64 getKey() const;
    U64 calculateKey() const;
    U64 getCheckingPieces() const;
    Square getEnPassant() const;
    U8 getHmClock() const;
    bool isCheck() const;
    bool isDoubleCheck() const;
    void setEnPassant(Square sq);
    void updateCapturedPieces(Square sq, Color c, PieceType p);
    void handlePawnMoves(Square from, Square to, MoveType movetype, Move mv);

    int mgMaterial() const;
    int egMaterial() const;
    int mgPieceSqBonus() const;
    int egPieceSqBonus() const;
    int scaleFactor() const;

    int pawnsEval(Phase) const;
    int mgEval() const;
    int egEval() const;

    template <bool>
    int eval() const;

    void make(Move);
    void unmake();
    void makeNull();
    void unmmakeNull();
    template <bool>
    void makeCastle(Square, Square, Color);
    bool isPseudoLegalMoveLegal(Move) const;
    bool isCheckingMove(Move) const;
    std::string toFEN() const;
    std::string DebugString() const;
    friend std::ostream& operator<<(std::ostream& os, const Chess& chess);

    friend class MoveGenerator;
};

template <bool forward>
inline void Chess::addPiece(Square sq, Color c, PieceType pt) {
    board.addPiece(sq, c, pt);
    mgMaterialScore += Eval::pieceValue(MIDGAME, c, pt);
    egMaterialScore += Eval::pieceValue(ENDGAME, c, pt);
    mgPieceSqScore += Eval::pieceSqBonus(MIDGAME, c, pt, sq);
    egPieceSqScore += Eval::pieceSqBonus(ENDGAME, c, pt, sq);

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][pt][sq];
    }
}

template <bool forward>
inline void Chess::removePiece(Square sq, Color c, PieceType pt) {
    board.removePiece(sq, c, pt);
    mgMaterialScore -= Eval::pieceValue(MIDGAME, c, pt);
    egMaterialScore -= Eval::pieceValue(ENDGAME, c, pt);
    mgPieceSqScore -= Eval::pieceSqBonus(MIDGAME, c, pt, sq);
    egPieceSqScore -= Eval::pieceSqBonus(ENDGAME, c, pt, sq);

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][pt][sq];
    }
}

template <bool forward>
inline void Chess::movePiece(Square from, Square to, Color c, PieceType pt) {
    board.movePiece(from, to, c, pt);
    mgPieceSqScore +=
        Eval::pieceSqBonus(MIDGAME, c, pt, to) - Eval::pieceSqBonus(MIDGAME, c, pt, from);
    egPieceSqScore +=
        Eval::pieceSqBonus(ENDGAME, c, pt, to) - Eval::pieceSqBonus(ENDGAME, c, pt, from);

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][pt][from] ^ Zobrist::psq[c][pt][to];
    }
}

inline void Chess::updateState(bool checkingMove = true) {
    // Update the incrementally updated state helper variables
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

inline U64 Chess::getKey() const {
    // Return the Zobrist key for the current board state
    return state.at(ply).zkey;
}

inline U64 Chess::calculateKey() const {
    // Calculate the Zobrist key for the current board state
    U64 zkey = 0x0;

    for (auto sq = A1; sq != INVALID; sq++) {
        auto piece = board.getPiece(sq);
        if (piece != NO_PIECE) {
            zkey ^= Zobrist::psq[Defs::getPieceColor(piece)][Defs::getPieceType(piece)][sq];
        }
    }

    if (turn == BLACK) zkey ^= Zobrist::stm;
    if (state.at(ply).canCastleOO(WHITE)) zkey ^= Zobrist::castle[WHITE][KINGSIDE];
    if (state.at(ply).canCastleOOO(WHITE)) zkey ^= Zobrist::castle[WHITE][QUEENSIDE];
    if (state.at(ply).canCastleOO(BLACK)) zkey ^= Zobrist::castle[BLACK][KINGSIDE];
    if (state.at(ply).canCastleOOO(BLACK)) zkey ^= Zobrist::castle[BLACK][QUEENSIDE];
    auto sq = getEnPassant();
    if (sq != INVALID) zkey ^= Zobrist::ep[Defs::fileFromSq(sq)];

    return zkey;
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

inline int Chess::mgMaterial() const { return mgMaterialScore; }
inline int Chess::egMaterial() const { return egMaterialScore; }
inline int Chess::mgPieceSqBonus() const { return mgPieceSqScore; }
inline int Chess::egPieceSqBonus() const { return egPieceSqScore; }

#endif