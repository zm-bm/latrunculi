#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "constants.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "score.hpp"
#include "state.hpp"
#include "zobrist.hpp"

template <GenType T>
class MoveGenerator;

class Thread;

class Chess {
   private:
    std::vector<State> state = {State()};
    U64 piecesBB[N_COLORS][N_PIECES] = {0};
    Piece squares[N_SQUARES] = {Piece::NONE};
    Square kingSquare[N_COLORS] = {E1, E8};
    U8 pieceCount[N_COLORS][N_PIECES] = {0};
    Color turn = WHITE;
    U32 ply = 0;
    U32 moveCounter = 0;
    Score material = {0, 0};
    Score psqBonus = {0, 0};
    Thread* thread;

   public:
    Chess() = default;
    explicit Chess(const std::string&, Thread* thread = nullptr);
    void loadFEN(const std::string&);

    template <bool = false>
    int eval() const;

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

    void updateState();
    void handlePieceCapture(Square sq, Color c, PieceType p);
    void handlePawnMoves(Square from, Square to, MoveType movetype, Move mv);
    void setEnPassant(Square sq);

    // accessors
    template <PieceType p>
    int count(Color c) const;
    template <PieceType... Ps>
    U64 pieces(Color c) const;
    Piece pieceOn(Square sq) const;
    PieceType pieceTypeOn(Square sq) const;
    Square kingSq(Color c) const;
    Color sideToMove() const;

    // movegen helpers
    U64 occupancy() const;
    U64 attacksTo(Square, Color) const;
    U64 attacksTo(Square, Color, U64) const;
    U64 calculateCheckBlockers(Color, Color) const;
    U64 calculateDiscoveredCheckers(Color c) const;
    U64 calculatePinnedPieces(Color c) const;
    U64 calculateCheckingPieces(Color c) const;
    bool isBitboardAttacked(U64, Color) const;

    // accessors
    State& getState() { return state.at(ply); }
    U64 getKey() const { return state.at(ply).zkey; }
    U64 getCheckingPieces() const { return state.at(ply).checkingPieces; }
    Square getEnPassant() const { return state.at(ply).enPassantSq; }
    U8 getHmClock() const { return state.at(ply).hmClock; }
    bool isCheck() const { return getCheckingPieces(); }
    bool isDoubleCheck() const { return BB::hasMoreThanOne(getCheckingPieces()); }
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

    template <bool debug>
    friend class Evaluator;
};

template <bool forward>
inline void Chess::addPiece(Square sq, Color c, PieceType p) {
    piecesBB[c][ALL_PIECES] ^= BB::set(sq);
    piecesBB[c][p] ^= BB::set(sq);
    squares[sq] = makePiece(c, p);
    pieceCount[c][p]++;
    material += pieceScore(p, c);
    psqBonus += Score{Eval::psqValue(MIDGAME, c, p, sq), Eval::psqValue(ENDGAME, c, p, sq)};

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][sq];
    }
}

template <bool forward>
inline void Chess::removePiece(Square sq, Color c, PieceType p) {
    piecesBB[c][ALL_PIECES] ^= BB::set(sq);
    piecesBB[c][p] ^= BB::set(sq);
    squares[sq] = Piece::NONE;
    pieceCount[c][p]--;
    material -= pieceScore(p, c);
    psqBonus -= Score{Eval::psqValue(MIDGAME, c, p, sq), Eval::psqValue(ENDGAME, c, p, sq)};

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][sq];
    }
}

template <bool forward>
inline void Chess::movePiece(Square from, Square to, Color c, PieceType p) {
    U64 mask = BB::set(from) | BB::set(to);
    piecesBB[c][ALL_PIECES] ^= mask;
    piecesBB[c][p] ^= mask;
    squares[from] = Piece::NONE;
    squares[to] = makePiece(c, p);
    psqBonus += Score{Eval::psqValue(MIDGAME, c, p, to) - Eval::psqValue(MIDGAME, c, p, from),
                      Eval::psqValue(ENDGAME, c, p, to) - Eval::psqValue(ENDGAME, c, p, from)};

    if (forward) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][from] ^ Zobrist::psq[c][p][to];
    }
}

// Return the count of pieces of a specific type for the given color
template <PieceType p>
inline int Chess::count(Color c) const {
    return pieceCount[c][p];
}

// Return the bitboard of pieces of a specific piece type for the given color
template <PieceType... Ps>
inline U64 Chess::pieces(Color c) const {
    return (piecesBB[c][Ps] | ...);
}

// Return the piece located at a specific square
inline Piece Chess::pieceOn(Square sq) const { return squares[sq]; }

// Return the type of the piece located at a specific square
inline PieceType Chess::pieceTypeOn(Square sq) const { return pieceTypeOf(squares[sq]); }

// Return the square of the king for the given color
inline Square Chess::kingSq(Color c) const { return kingSquare[c]; }

// Return the color of the side to move
inline Color Chess::sideToMove() const { return turn; }

// Return the combined bitboard of all pieces on the board
inline U64 Chess::occupancy() const {
    return pieces<ALL_PIECES>(WHITE) | pieces<ALL_PIECES>(BLACK);
}

// Returns a bitboard of pieces of color c which attacks a square
inline U64 Chess::attacksTo(Square sq, Color c) const {
    return attacksTo(sq, c, occupancy());
}

inline U64 Chess::attacksTo(Square sq, Color c, U64 occ) const {
    // Returns a bitboard of pieces of color c which attacks a square
    U64 piece = BB::set(sq);

    return (pieces<PAWN>(c) & BB::pawnAttacks(piece, ~c)) |
           (pieces<KNIGHT>(c) & BB::pieceMoves<KNIGHT>(sq, occ)) |
           (pieces<KING>(c) & BB::pieceMoves<KING>(sq, occ)) |
           (pieces<BISHOP, QUEEN>(c) & BB::pieceMoves<BISHOP>(sq, occ)) |
           (pieces<ROOK, QUEEN>(c) & BB::pieceMoves<ROOK>(sq, occ));
}

inline U64 Chess::calculateCheckBlockers(Color c, Color kingC) const {
    // Determine pieces of color c, which block the color kingC from attack by
    // the enemy
    Color enemy = ~kingC;
    Square king = kingSq(kingC);

    // Determine which enemy sliders could check the kingC's king
    U64 blockers = 0;
    U64 pinners = (BB::pieceMoves<BISHOP>(king) & pieces<BISHOP, QUEEN>(enemy)) |
                  (BB::pieceMoves<ROOK>(king) & pieces<ROOK, QUEEN>(enemy));

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

inline U64 Chess::calculateDiscoveredCheckers(Color c) const {
    // Calculate and return the bitboard of pieces that can give discovered
    // check
    return calculateCheckBlockers(c, ~c);
}

inline U64 Chess::calculatePinnedPieces(Color c) const {
    // Calculate and return the bitboard of pieces that are pinned relative to
    // the king
    return calculateCheckBlockers(c, c);
}

inline U64 Chess::calculateCheckingPieces(Color c) const {
    // Calculate and return the bitboard of pieces attacking the king
    return attacksTo(kingSq(c), ~c);
}

inline bool Chess::isBitboardAttacked(U64 bitboard, Color c) const {
    // Determine if any set square of a bitboard is attacked by color c
    while (bitboard) {
        Square sq = BB::lsb(bitboard);
        bitboard &= BB::clear(sq);

        if (attacksTo(sq, c)) return true;
    }

    return false;
}

inline void Chess::updateState() {
    Color enemy = ~turn;
    Square king = kingSq(enemy);
    U64 occ = occupancy();

    state[ply].blockers[WHITE] = calculatePinnedPieces(WHITE);
    state[ply].blockers[BLACK] = calculatePinnedPieces(BLACK);
    state[ply].pinners[WHITE] = calculateDiscoveredCheckers(WHITE);
    state[ply].pinners[BLACK] = calculateDiscoveredCheckers(BLACK);

    state[ply].checkingPieces = calculateCheckingPieces(turn);
    state[ply].checkingSquares[PAWN] = BB::pawnAttacks(BB::set(king), enemy);
    state[ply].checkingSquares[KNIGHT] = BB::pieceMoves<KNIGHT>(king, occ);
    state[ply].checkingSquares[BISHOP] = BB::pieceMoves<BISHOP>(king, occ);
    state[ply].checkingSquares[ROOK] = BB::pieceMoves<ROOK>(king, occ);
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

    auto doubleMove = static_cast<U8>(DOUBLE);
    if ((from - to) == doubleMove || (to - from) == doubleMove) {
        setEnPassant(pawnMove<PUSH, false>(to, turn));
    } else if (movetype == PROMOTION) {
        removePiece<true>(to, turn, PAWN);
        addPiece<true>(to, turn, mv.promoPiece());
    }
}

inline void Chess::setEnPassant(Square sq) {
    // Set the en passant target square and update the hash key
    state.at(ply).enPassantSq = sq;
    state.at(ply).zkey ^= Zobrist::ep[fileOf(sq)];
}

inline U64 Chess::calculateKey() const {
    // Calculate the Zobrist key for the current board state
    U64 zkey = 0x0;

    for (auto sq = A1; sq != INVALID; sq++) {
        auto piece = pieceOn(sq);
        if (piece != Piece::NONE) {
            zkey ^= Zobrist::psq[pieceColorOf(piece)][pieceTypeOf(piece)][sq];
        }
    }

    if (turn == BLACK) zkey ^= Zobrist::stm;
    if (state.at(ply).canCastleOO(WHITE)) zkey ^= Zobrist::castle[WHITE][KINGSIDE];
    if (state.at(ply).canCastleOOO(WHITE)) zkey ^= Zobrist::castle[WHITE][QUEENSIDE];
    if (state.at(ply).canCastleOO(BLACK)) zkey ^= Zobrist::castle[BLACK][KINGSIDE];
    if (state.at(ply).canCastleOOO(BLACK)) zkey ^= Zobrist::castle[BLACK][QUEENSIDE];
    auto sq = getEnPassant();
    if (sq != INVALID) zkey ^= Zobrist::ep[fileOf(sq)];

    return zkey;
}
