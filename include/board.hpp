#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "base.hpp"
#include "bb.hpp"
#include "constants.hpp"
#include "move.hpp"
#include "score.hpp"
#include "state.hpp"
#include "types.hpp"
#include "zobrist.hpp"

template <MoveGenMode T>
class MoveGenerator;

class Thread;

class Board {
   private:
    U64 piecesBB[N_COLORS][N_PIECES]  = {0};
    U8 pieceCount[N_COLORS][N_PIECES] = {0};
    Piece squares[N_SQUARES]          = {Piece::None};
    Square kingSquare[N_COLORS]       = {E1, E8};
    Color turn                        = WHITE;
    Score material                    = {0, 0};
    Score psqBonus                    = {0, 0};
    std::vector<State> state          = {State()};
    U32 ply                           = 0;
    U32 moveCounter                   = 0;
    Thread* thread                    = nullptr;

   public:
    // constructors
    Board() = default;
    explicit Board(const std::string& fen);

    Board(const Board&)            = delete;
    Board& operator=(const Board&) = delete;

    // accessors
    template <PieceType... Ps>
    U64 pieces() const;
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
    U64 blockers(Color c) const;
    Square enPassantSq() const;
    U8 halfmove() const;

    // move/check properties
    int see(Move) const;
    bool isDraw() const;
    bool isLegalMove(Move) const;
    bool isCheckingMove(Move) const;
    bool isCapture(Move move) const;
    bool isCheck() const;
    bool isDoubleCheck() const;

    // attacks to squares/bitboards
    U64 attacksTo(Square, U64) const;
    U64 attacksTo(Square) const;
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

    // perft
    template <NodeType = NodeType::Root>
    U64 perft(int, std::ostream& = std::cout);

    // FEN / string conversions
    void loadFEN(const std::string&);
    std::string toFEN() const;
    std::string toSAN(Move) const;

    void setThread(Thread* t) { thread = t; }
};

inline Board::Board(const std::string& fen) { loadFEN(fen); }

template <PieceType... Ps>
inline U64 Board::pieces() const {
    return ((piecesBB[WHITE][idx(Ps)] | piecesBB[BLACK][idx(Ps)]) | ...);
};

template <PieceType... Ps>
inline U64 Board::pieces(Color c) const {
    return (piecesBB[c][idx(Ps)] | ...);
};

inline U64 Board::occupancy() const { return pieces<PieceType::All>(); }

inline U8 Board::count(Color c, PieceType p) const { return pieceCount[c][idx(p)]; };

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

inline U64 Board::blockers(Color c) const { return state.at(ply).blockers[c]; }

inline Square Board::enPassantSq() const { return state.at(ply).enPassantSq; }

inline U8 Board::halfmove() const { return state.at(ply).hmClock; }

inline bool Board::isCapture(Move move) const { return pieceTypeOn(move.to()) != PieceType::None; };

inline bool Board::isCheck() const { return checkers(); }

inline bool Board::isDoubleCheck() const { return BB::isMany(checkers()); }

// Returns a bitboard of pieces of color c which attacks a square
inline U64 Board::attacksTo(Square sq, Color c) const { return attacksTo(sq, c, occupancy()); }

// Returns a bitboard of pieces of color c which attacks a square
inline U64 Board::attacksTo(Square sq, Color c, U64 occupied) const {
    auto diagSliders = pieces<PieceType::Bishop, PieceType::Queen>(c);
    auto lineSliders = pieces<PieceType::Rook, PieceType::Queen>(c);
    return (pieces<PieceType::Pawn>(c) & BB::pawnAttacks(BB::set(sq), ~c)) |
           (pieces<PieceType::Knight>(c) & BB::moves<PieceType::Knight>(sq, occupied)) |
           (pieces<PieceType::King>(c) & BB::moves<PieceType::King>(sq, occupied)) |
           (diagSliders & BB::moves<PieceType::Bishop>(sq, occupied)) |
           (lineSliders & BB::moves<PieceType::Rook>(sq, occupied));
}

// Returns a bitboard of pieces of color c which attacks a square
inline U64 Board::attacksTo(Square sq) const { return attacksTo(sq, occupancy()); }

// Returns a bitboard of pieces of any which attacks a square
inline U64 Board::attacksTo(Square sq, U64 occupied) const {
    auto diagSliders = pieces<PieceType::Bishop, PieceType::Queen>();
    auto lineSliders = pieces<PieceType::Rook, PieceType::Queen>();
    return (pieces<PieceType::Pawn>(WHITE) & BB::pawnAttacks(BB::set(sq), BLACK)) |
           (pieces<PieceType::Pawn>(BLACK) & BB::pawnAttacks(BB::set(sq), WHITE)) |
           (pieces<PieceType::Knight>() & BB::moves<PieceType::Knight>(sq, occupied)) |
           (pieces<PieceType::King>() & BB::moves<PieceType::King>(sq, occupied)) |
           (diagSliders & BB::moves<PieceType::Bishop>(sq, occupied)) |
           (lineSliders & BB::moves<PieceType::Rook>(sq, occupied));
}

// Determine if any set square of a bitboard is attacked by color c
inline U64 Board::attacksTo(U64 bitboard, Color c) const {
    U64 attacks  = 0;
    U64 occupied = occupancy();
    while (bitboard) {
        attacks |= attacksTo(BB::lsbPop(bitboard), c, occupied);
    }
    return attacks;
}

template <bool applyHash>
inline void Board::addPiece(Square sq, Color c, PieceType pt) {
    pieceCount[c][idx(pt)]++;
    piecesBB[c][idx(pt)]       ^= BB::set(sq);
    piecesBB[c][PieceIdx::All] ^= BB::set(sq);
    squares[sq]                 = makePiece(c, pt);
    material                   += pieceScore(pt, c);
    psqBonus                   += pieceSqScore(pt, c, sq);
    if constexpr (applyHash) state.at(ply).zkey ^= Zobrist::hashPiece(c, pt, sq);
}

template <bool applyHash>
inline void Board::removePiece(Square sq, Color c, PieceType pt) {
    pieceCount[c][idx(pt)]--;
    piecesBB[c][idx(pt)]       ^= BB::set(sq);
    piecesBB[c][PieceIdx::All] ^= BB::set(sq);
    squares[sq]                 = Piece::None;
    material                   -= pieceScore(pt, c);
    psqBonus                   -= pieceSqScore(pt, c, sq);
    if constexpr (applyHash) state.at(ply).zkey ^= Zobrist::hashPiece(c, pt, sq);
}

template <bool applyHash>
inline void Board::movePiece(Square from, Square to, Color c, PieceType pt) {
    U64 mask                    = BB::set(from) | BB::set(to);
    piecesBB[c][idx(pt)]       ^= mask;
    piecesBB[c][PieceIdx::All] ^= mask;
    squares[from]               = Piece::None;
    squares[to]                 = makePiece(c, pt);
    psqBonus                   += pieceSqScore(pt, c, to) - pieceSqScore(pt, c, from);

    if constexpr (applyHash) {
        state.at(ply).zkey ^= Zobrist::hashPiece(c, pt, from) ^ Zobrist::hashPiece(c, pt, to);
    }
}

// Calculate the Zobrist key for the current board state
inline U64 Board::calculateKey() const {
    U64 zkey = 0x0;

    for (auto sq = A1; sq != INVALID; ++sq) {
        auto piece = pieceOn(sq);
        if (piece != Piece::None) {
            zkey ^= Zobrist::hashPiece(pieceColorOf(piece), pieceTypeOf(piece), sq);
        }
    }

    if (turn == BLACK) zkey ^= Zobrist::stm;
    if (canCastleOO(WHITE)) zkey ^= Zobrist::hashCastle(WHITE, Castle::KingSide);
    if (canCastleOOO(WHITE)) zkey ^= Zobrist::hashCastle(WHITE, Castle::QueenSide);
    if (canCastleOO(BLACK)) zkey ^= Zobrist::hashCastle(BLACK, Castle::KingSide);
    if (canCastleOOO(BLACK)) zkey ^= Zobrist::hashCastle(BLACK, Castle::QueenSide);
    auto sq = enPassantSq();
    if (sq != INVALID) zkey ^= Zobrist::hashEp(sq);

    return zkey;
}

inline bool Board::canCastle(Color c) const {
    // Check if the specified color can castle
    auto rights = castleRights() & (c ? WHITE_CASTLE : BLACK_CASTLE);
    return rights.any();
}

inline bool Board::canCastleOO(Color c) const {
    // Check if the specified color can castle kingside (OO)
    auto rights = castleRights() & (c ? WHITE_OO : BLACK_OO);
    return rights.any();
}

inline bool Board::canCastleOOO(Color c) const {
    // Check if the specified color can castle queenside (OOO)
    auto rights = castleRights() & (c ? WHITE_OOO : BLACK_OOO);
    return rights.any();
}

inline void Board::disableCastle(Color c) {
    // Disable castling for the specified color
    if (canCastleOO(c)) state.at(ply).zkey ^= Zobrist::hashCastle(c, Castle::KingSide);
    if (canCastleOOO(c)) state.at(ply).zkey ^= Zobrist::hashCastle(c, Castle::QueenSide);
    state.at(ply).castle &= c ? BLACK_CASTLE : WHITE_CASTLE;
}

inline void Board::disableCastle(Color c, Square sq) {
    // Disable casting for the specified color and side
    if (sq == RookOrigin[idx(Castle::KingSide)][c] && canCastleOO(c)) {
        state.at(ply).zkey   ^= Zobrist::hashCastle(c, Castle::KingSide);
        state.at(ply).castle &= ~(c ? WHITE_OO : BLACK_OO);
    } else if (sq == RookOrigin[idx(Castle::QueenSide)][c] && canCastleOOO(c)) {
        state.at(ply).zkey   ^= Zobrist::hashCastle(c, Castle::QueenSide);
        state.at(ply).castle &= ~(c ? WHITE_OOO : BLACK_OOO);
    }
}

// Update enemy pinning pieces and all blocking pieces for the king of a given color
inline void Board::updatePinInfo(Color c) {
    Color enemy = ~c;
    Square king = kingSq(c);
    U64 sliders =
        (BB::moves<PieceType::Bishop>(king) & pieces<PieceType::Bishop, PieceType::Queen>(enemy)) |
        (BB::moves<PieceType::Rook>(king) & pieces<PieceType::Rook, PieceType::Queen>(enemy));

    U64 occupied = occupancy();
    U64 blockers = 0;
    U64 pinners  = 0;
    while (sliders) {
        // For each potential pinning piece
        Square pinner       = BB::lsbPop(sliders);
        U64 piecesInBetween = occupied & BB::between(king, pinner);

        // Check if only one piece separates the slider and the king
        if (!BB::isMany(piecesInBetween)) {
            state[ply].pinners[enemy] |= BB::set(pinner);
            state[ply].blockers[c]    |= piecesInBetween & occupied;
        }
    }
}

inline void Board::updateCheckInfo() {
    Color enemy      = ~turn;
    Square enemyKing = kingSq(enemy);
    U64 occupied     = occupancy();

    state[ply].checkers = attacksTo(kingSq(turn), enemy);

    state[ply].checks[PieceIdx::Pawn]   = BB::pawnAttacks(BB::set(enemyKing), enemy);
    state[ply].checks[PieceIdx::Knight] = BB::moves<PieceType::Knight>(enemyKing, occupied);
    state[ply].checks[PieceIdx::Bishop] = BB::moves<PieceType::Bishop>(enemyKing, occupied);
    state[ply].checks[PieceIdx::Rook]   = BB::moves<PieceType::Rook>(enemyKing, occupied);
    state[ply].checks[PieceIdx::Queen] =
        state[ply].checks[PieceIdx::Bishop] | state[ply].checks[PieceIdx::Rook];

    updatePinInfo(WHITE);
    updatePinInfo(BLACK);
}

std::ostream& operator<<(std::ostream& os, const Board& board);
