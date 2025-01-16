#ifndef LATRUNCULI_CHESS_H
#define LATRUNCULI_CHESS_H

#include <string>
#include <tuple>
#include <vector>

#include "board.hpp"
#include "constants.hpp"
#include "eval.hpp"
#include "score.hpp"
#include "state.hpp"
#include "zobrist.hpp"

class Chess {
   private:
    std::vector<State> state = {State()};
    Board board = Board(STARTFEN);
    Color turn = WHITE;
    U32 ply = 0;
    U32 moveCounter = 0;
    Score material = {0, 0};
    Score psqBonus = {0, 0};

   public:
    explicit Chess(const std::string&);

    // evaluation
    template <bool>
    int eval() const;
    Score pawnsEval() const;
    Score piecesEval() const;
    int scaleFactor() const;

    // eval helpers
    int nonPawnMaterial(Color) const;
    int minorsBehindPawns() const;
    bool hasOppositeBishops() const;
    U64 passedPawns(Color) const;

    // make move / mutators
    void make(Move);
    void unmake();
    void makeNull();
    void unmmakeNull();

    // make move helpers
    template <bool>
    void addPiece(Square, Color, PieceType);
    template <bool>
    void removePiece(Square, Color, PieceType);
    template <bool>
    void movePiece(Square, Square, Color, PieceType);
    template <bool>
    void moveCastle(Square, Square, Color);
    void updateState(bool);
    void handlePieceCapture(Square sq, Color c, PieceType p);
    void handlePawnMoves(Square from, Square to, MoveType movetype, Move mv);
    void setEnPassant(Square sq);

    // accessors
    U64 getKey() const { return state.at(ply).zkey; }
    U64 getCheckingPieces() const { return state.at(ply).checkingPieces; }
    Square getEnPassant() const { return state.at(ply).enPassantSq; }
    U8 getHmClock() const { return state.at(ply).hmClock; }
    bool isCheck() const { return getCheckingPieces(); }
    bool isDoubleCheck() const { return BB::moreThanOneSet(getCheckingPieces()); }
    Score materialScore() const { return material; }
    Score psqBonusScore() const { return psqBonus; }

    // other helpers
    U64 calculateKey() const;
    bool isLegalMove(Move) const;
    bool isCheckingMove(Move) const;

    // string helpers
    std::string toFEN() const;
    std::string DebugString() const;
    friend std::ostream& operator<<(std::ostream& os, const Chess& chess);

    friend class MoveGenerator;
};

inline int Chess::nonPawnMaterial(Color c) const {
    return (board.count<KNIGHT>(c) * Eval::mgPieceValue(KNIGHT) +
            board.count<BISHOP>(c) * Eval::mgPieceValue(BISHOP) +
            board.count<ROOK>(c) * Eval::mgPieceValue(ROOK) +
            board.count<QUEEN>(c) * Eval::mgPieceValue(QUEEN));
}

inline int Chess::minorsBehindPawns() const {
    U64 wMinorsBehind = BB::movesByPawns<PawnMove::PUSH, BLACK>(board.getPieces<PAWN>(WHITE)) &
                        (board.getPieces<KNIGHT>(WHITE) | board.getPieces<BISHOP>(WHITE));
    U64 bMinorsBehind = BB::movesByPawns<PawnMove::PUSH, WHITE>(board.getPieces<PAWN>(BLACK)) &
                        (board.getPieces<KNIGHT>(BLACK) | board.getPieces<BISHOP>(BLACK));
    return BB::bitCount(wMinorsBehind) - BB::bitCount(bMinorsBehind);
}

inline bool Chess::hasOppositeBishops() const {
    if (board.count<BISHOP>(WHITE) != 1 || board.count<BISHOP>(BLACK) != 1) {
        return false;
    }
    return Eval::oppositeBishops(board.getPieces<BISHOP>(WHITE), board.getPieces<BISHOP>(BLACK));
}

inline U64 Chess::passedPawns(Color c) const {
    U64 enemyPawns = board.getPieces<PAWN>(~c);
    U64 blockers = (c == WHITE) ? BB::getAllFrontSpan<BLACK>(enemyPawns)
                                : BB::getAllFrontSpan<WHITE>(enemyPawns);
    return board.getPieces<PAWN>(c) & ~blockers;
}

template <bool forward>
inline void Chess::addPiece(Square sq, Color c, PieceType pt) {
    board.addPiece(sq, c, pt);
    material += Score{Eval::pieceValue(MIDGAME, c, pt), Eval::pieceValue(ENDGAME, c, pt)};
    psqBonus += Score{Eval::psqValue(MIDGAME, c, pt, sq), Eval::psqValue(ENDGAME, c, pt, sq)};

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][pt][sq];
    }
}

template <bool forward>
inline void Chess::removePiece(Square sq, Color c, PieceType pt) {
    board.removePiece(sq, c, pt);
    material -= Score{Eval::pieceValue(MIDGAME, c, pt), Eval::pieceValue(ENDGAME, c, pt)};
    psqBonus -= Score{Eval::psqValue(MIDGAME, c, pt, sq), Eval::psqValue(ENDGAME, c, pt, sq)};

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][pt][sq];
    }
}

template <bool forward>
inline void Chess::movePiece(Square from, Square to, Color c, PieceType pt) {
    board.movePiece(from, to, c, pt);
    psqBonus += Score{Eval::psqValue(MIDGAME, c, pt, to) - Eval::psqValue(MIDGAME, c, pt, from),
                      Eval::psqValue(ENDGAME, c, pt, to) - Eval::psqValue(ENDGAME, c, pt, from)};

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

inline void Chess::handlePieceCapture(Square sq, Color c, PieceType p) {
    // Reset half move clock for capture
    state[ply].hmClock = 0;
    removePiece<true>(sq, c, p);

    // Disable castle rights if captured piece is rook
    if (state.at(ply).canCastle(c) && p == ROOK) {
        state.at(ply).disableCastle(c, sq);
    }
}

inline void Chess::handlePawnMoves(Square from, Square to, MoveType movetype, Move mv) {
    // Reset half move clock for pawn move
    state[ply].hmClock = 0;

    auto doubleMove = static_cast<U8>(PawnMove::DOUBLE);
    if ((from - to) == doubleMove || (to - from) == doubleMove) {
        setEnPassant(Defs::pawnMove<PawnMove::PUSH, false>(to, turn));
    } else if (movetype == PROMOTION) {
        removePiece<true>(to, turn, PAWN);
        addPiece<true>(to, turn, mv.promoPiece());
    }
}

inline void Chess::setEnPassant(Square sq) {
    // Set the en passant target square and update the hash key
    state.at(ply).enPassantSq = sq;
    state.at(ply).zkey ^= Zobrist::ep[Defs::fileFromSq(sq)];
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

#endif