#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "bb.hpp"
#include "constants.hpp"
#include "move.hpp"
#include "score.hpp"
#include "state.hpp"
#include "zobrist.hpp"

template <GenType T>
class MoveGenerator;

class Thread;

class Board {
   private:
    U64 piecesBB[N_COLORS][N_PIECES]  = {0};
    U8 pieceCount[N_COLORS][N_PIECES] = {0};
    Piece squares[N_SQUARES]          = {Piece::NONE};
    Square kingSquare[N_COLORS]       = {E1, E8};
    Color turn                        = WHITE;
    Score material                    = {0, 0};
    Score psqBonus                    = {0, 0};
    std::vector<State> state          = {State()};
    U32 ply                           = 0;
    U32 moveCounter                   = 0;
    Thread* thread;

   public:
    Board() = default;
    explicit Board(const std::string&, Thread* thread = nullptr);

    // accessors
    template <PieceType... Ps>
    U64 pieces(Color c) const;
    U64 occupancy() const;
    U8 count(Color c, PieceType p) const;
    Piece pieceOn(Square sq) const;
    Piece pieceOn(File file, Rank rank) const;
    PieceType pieceTypeOn(Square sq) const;
    Square kingSq(Color c) const;
    Color sideToMove() const;
    Score materialScore() const;
    Score psqBonusScore() const;
    State& getState();
    CastleRights castleRights() const;
    U64 checkers() const;
    Square enPassantSq() const;
    U8 halfmove() const;

    // move/check properties
    bool isLegalMove(Move) const;
    bool isCheckingMove(Move) const;
    bool isCapture(Move move) const;
    bool isCheck() const;
    bool isDoubleCheck() const;

    // attacks to squares/bitboards
    U64 attacksTo(Square, Color, U64) const;
    U64 attacksTo(Square, Color) const;
    U64 attacksTo(U64, Color) const;

    // piece modifiers
    template <bool>
    void addPiece(Square, Color, PieceType);
    template <bool>
    void removePiece(Square, Color, PieceType);
    template <bool>
    void movePiece(Square, Square, Color, PieceType);

    // zobrist keys
    U64 getKey() const { return state.at(ply).zkey; }
    U64 calculateKey() const;

    // castling
    bool canCastle(Color c) const;
    bool canCastleOO(Color c) const;
    bool canCastleOOO(Color c) const;
    void disableCastle(Color c);
    void disableCastle(Color c, Square sq);

    // check info updaters
    void updateCheckInfo();
    void updatePinInfo(Color);

    // make moves
    void make(Move);
    void unmake();
    void makeNull();
    void unmmakeNull();

    // FENs
    void loadFEN(const std::string&);
    std::string toFEN() const;
};

inline Board::Board(const std::string& fen, Thread* thread) : thread(thread) { loadFEN(fen); }

template <PieceType... Ps>
inline U64 Board::pieces(Color c) const {
    return (piecesBB[c][Ps] | ...);
};

inline U64 Board::occupancy() const {
    return pieces<ALL_PIECES>(WHITE) | pieces<ALL_PIECES>(BLACK);
}

inline U8 Board::count(Color c, PieceType p) const { return pieceCount[c][p]; };

inline Piece Board::pieceOn(Square sq) const { return squares[sq]; }

inline Piece Board::pieceOn(File file, Rank rank) const { return squares[makeSquare(file, rank)]; };

inline PieceType Board::pieceTypeOn(Square sq) const { return pieceTypeOf(squares[sq]); };

inline Square Board::kingSq(Color c) const { return kingSquare[c]; };

inline Color Board::sideToMove() const { return turn; }

inline Score Board::materialScore() const { return material; }

inline Score Board::psqBonusScore() const { return psqBonus; }

inline State& Board::getState() { return state.at(ply); }

inline CastleRights Board::castleRights() const { return state.at(ply).castle; }

inline U64 Board::checkers() const { return state.at(ply).checkers; }

inline Square Board::enPassantSq() const { return state.at(ply).enPassantSq; }

inline U8 Board::halfmove() const { return state.at(ply).hmClock; }

inline bool Board::isCapture(Move move) const { return pieceTypeOn(move.to()) != NO_PIECE_TYPE; };

inline bool Board::isCheck() const { return checkers(); }

inline bool Board::isDoubleCheck() const { return BB::hasMoreThanOne(checkers()); }

// Returns a bitboard of pieces of color c which attacks a square
inline U64 Board::attacksTo(Square sq, Color c) const { return attacksTo(sq, c, occupancy()); }

// Returns a bitboard of pieces of color c which attacks a square
inline U64 Board::attacksTo(Square sq, Color c, U64 occ) const {
    return (pieces<PAWN>(c) & BB::pawnAttacks(BB::set(sq), ~c)) |
           (pieces<KNIGHT>(c) & BB::pieceMoves<KNIGHT>(sq, occ)) |
           (pieces<KING>(c) & BB::pieceMoves<KING>(sq, occ)) |
           (pieces<BISHOP, QUEEN>(c) & BB::pieceMoves<BISHOP>(sq, occ)) |
           (pieces<ROOK, QUEEN>(c) & BB::pieceMoves<ROOK>(sq, occ));
}

// Determine if any set square of a bitboard is attacked by color c
inline U64 Board::attacksTo(U64 bitboard, Color c) const {
    U64 attacks = 0;
    U64 occ     = occupancy();
    while (bitboard) {
        attacks |= attacksTo(BB::lsbPop(bitboard), c, occ);
    }
    return attacks;
}

template <bool applyHash>
inline void Board::addPiece(Square sq, Color c, PieceType p) {
    pieceCount[c][p]++;
    piecesBB[c][ALL_PIECES] ^= BB::set(sq);
    piecesBB[c][p]          ^= BB::set(sq);
    squares[sq]              = makePiece(c, p);
    material                += pieceScore(p, c);
    psqBonus                += pieceSqScore(p, c, sq);
    if constexpr (applyHash) state.at(ply).zkey ^= Zobrist::psq[c][p][sq];
}

template <bool applyHash>
inline void Board::removePiece(Square sq, Color c, PieceType p) {
    pieceCount[c][p]--;
    piecesBB[c][ALL_PIECES] ^= BB::set(sq);
    piecesBB[c][p]          ^= BB::set(sq);
    squares[sq]              = Piece::NONE;
    material                -= pieceScore(p, c);
    psqBonus                -= pieceSqScore(p, c, sq);
    if constexpr (applyHash) state.at(ply).zkey ^= Zobrist::psq[c][p][sq];
}

template <bool applyHash>
inline void Board::movePiece(Square from, Square to, Color c, PieceType p) {
    U64 mask                 = BB::set(from) | BB::set(to);
    piecesBB[c][ALL_PIECES] ^= mask;
    piecesBB[c][p]          ^= mask;
    squares[from]            = Piece::NONE;
    squares[to]              = makePiece(c, p);
    psqBonus                += pieceSqScore(p, c, to) - pieceSqScore(p, c, from);

    if constexpr (applyHash) {
        state.at(ply).zkey ^= Zobrist::psq[c][p][from] ^ Zobrist::psq[c][p][to];
    }
}

// Calculate the Zobrist key for the current board state
inline U64 Board::calculateKey() const {
    U64 zkey = 0x0;

    for (auto sq = A1; sq != INVALID; sq++) {
        auto piece = pieceOn(sq);
        if (piece != Piece::NONE) {
            zkey ^= Zobrist::psq[pieceColorOf(piece)][pieceTypeOf(piece)][sq];
        }
    }

    if (turn == BLACK) zkey ^= Zobrist::stm;
    if (canCastleOO(WHITE)) zkey ^= Zobrist::castle[WHITE][KINGSIDE];
    if (canCastleOOO(WHITE)) zkey ^= Zobrist::castle[WHITE][QUEENSIDE];
    if (canCastleOO(BLACK)) zkey ^= Zobrist::castle[BLACK][KINGSIDE];
    if (canCastleOOO(BLACK)) zkey ^= Zobrist::castle[BLACK][QUEENSIDE];
    auto sq = enPassantSq();
    if (sq != INVALID) zkey ^= Zobrist::ep[fileOf(sq)];

    return zkey;
}

inline bool Board::canCastle(Color c) const {
    // Check if the specified color can castle
    return castleRights() & (c ? WHITE_CASTLE : BLACK_CASTLE);
}

inline bool Board::canCastleOO(Color c) const {
    // Check if the specified color can castle kingside (OO)
    return castleRights() & (c ? WHITE_OO : BLACK_OO);
}

inline bool Board::canCastleOOO(Color c) const {
    // Check if the specified color can castle queenside (OOO)
    return castleRights() & (c ? WHITE_OOO : BLACK_OOO);
}

inline void Board::disableCastle(Color c) {
    // Disable castling for the specified color
    if (canCastleOO(c)) state.at(ply).zkey ^= Zobrist::castle[c][KINGSIDE];
    if (canCastleOOO(c)) state.at(ply).zkey ^= Zobrist::castle[c][QUEENSIDE];
    state.at(ply).castle &= c ? BLACK_CASTLE : WHITE_CASTLE;
}

inline void Board::disableCastle(Color c, Square sq) {
    // Disable casting for the specified color and side
    if (sq == RookOrigin[KINGSIDE][c] && canCastleOO(c)) {
        state.at(ply).zkey   ^= Zobrist::castle[c][KINGSIDE];
        state.at(ply).castle &= ~(c ? WHITE_OO : BLACK_OO);
    } else if (sq == RookOrigin[QUEENSIDE][c] && canCastleOOO(c)) {
        state.at(ply).zkey   ^= Zobrist::castle[c][QUEENSIDE];
        state.at(ply).castle &= ~(c ? WHITE_OOO : BLACK_OOO);
    }
}

// Update enemy pinning pieces and all blocking pieces for the king of a given color
inline void Board::updatePinInfo(Color c) {
    Color enemy = ~c;
    Square king = kingSq(c);
    U64 sliders = (BB::pieceMoves<BISHOP>(king) & pieces<BISHOP, QUEEN>(enemy)) |
                  (BB::pieceMoves<ROOK>(king) & pieces<ROOK, QUEEN>(enemy));

    U64 occ      = occupancy();
    U64 blockers = 0;
    U64 pinners  = 0;
    while (sliders) {
        // For each potential pinning piece
        Square pinner       = BB::lsbPop(sliders);
        U64 piecesInBetween = occ & BB::betweenBB(king, pinner);

        // Check if only one piece separates the slider and the king
        if (!BB::hasMoreThanOne(piecesInBetween)) {
            state[ply].pinners[enemy] |= BB::set(pinner);
            state[ply].blockers[c]    |= piecesInBetween & occ;
        }
    }
}

inline void Board::updateCheckInfo() {
    Color enemy      = ~turn;
    Square enemyKing = kingSq(enemy);
    U64 occ          = occupancy();

    state[ply].checkers       = attacksTo(kingSq(turn), enemy);
    state[ply].checks[PAWN]   = BB::pawnAttacks(BB::set(enemyKing), enemy);
    state[ply].checks[KNIGHT] = BB::pieceMoves<KNIGHT>(enemyKing, occ);
    state[ply].checks[BISHOP] = BB::pieceMoves<BISHOP>(enemyKing, occ);
    state[ply].checks[ROOK]   = BB::pieceMoves<ROOK>(enemyKing, occ);
    state[ply].checks[QUEEN]  = state[ply].checks[BISHOP] | state[ply].checks[ROOK];
    updatePinInfo(WHITE);
    updatePinInfo(BLACK);
}

std::ostream& operator<<(std::ostream& os, const Board& chess);
