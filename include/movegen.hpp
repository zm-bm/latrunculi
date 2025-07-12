#pragma once

#include <array>
#include <iostream>
#include <optional>

#include "bb.hpp"
#include "board.hpp"
#include "eval.hpp"
#include "magic.hpp"
#include "move.hpp"
#include "move_order.hpp"
#include "thread.hpp"
#include "tt.hpp"
#include "types.hpp"

template <MoveGenMode T>
class MoveGenerator {
   public:
    MoveGenerator(const Board& board);

    Move& operator[](int index) { return moves[index]; }
    const Move* begin() { return moves.begin(); }
    const Move* end() { return last; }
    const bool empty() { return last == moves.data(); }
    const size_t size() { return static_cast<std::size_t>(last - moves.data()); }
    void sort(MoveOrder&);

   private:
    const Board& board;
    std::array<Move, MAX_MOVES> moves;
    Move* last;

    void add(Square, Square, MoveType = MoveType::Normal, PieceType = PieceType::Knight);

    template <MoveGenMode>
    void generate();
    template <MoveGenMode, Color>
    void generateMoves();
    template <MoveGenMode, Color>
    void generatePawnMoves(U64, U64);
    template <PieceType, Color>
    void generatePieceMoves(U64, U64);
    template <PawnMove, Color>
    void addPawnMoves(U64);
    template <PawnMove, Color>
    void addPawnPromotions(U64);
    template <PawnMove, Color>
    void addEnPassants(U64, Square);

    bool canCastleOO(U64 occupied, Color turn);
    bool canCastleOOO(U64 occupied, Color turn);
};

template <MoveGenMode T>
MoveGenerator<T>::MoveGenerator(const Board& board) : board(board), last(moves.data()) {
    if (board.isCheck()) {
        generate<MoveGenMode::Evasions>();
    } else {
        generate<T>();
    }
}

template <MoveGenMode T>
inline void MoveGenerator<T>::add(Square from, Square to, MoveType mtype, PieceType promoPiece) {
    new (last++) Move(from, to, mtype, promoPiece);
}

template <MoveGenMode T>
inline void MoveGenerator<T>::sort(MoveOrder& moveOrder) {
    for (Move* move = moves.data(); move != last; ++move) {
        move->priority = moveOrder.scoreMove(*move);
    }

    auto comp = [](const Move& a, const Move& b) { return a.priority > b.priority; };
    std::stable_sort(moves.begin(), last, comp);
}

template <MoveGenMode T>
template <MoveGenMode G>
void MoveGenerator<T>::generate() {
    if (board.sideToMove() == WHITE) {
        generateMoves<G, WHITE>();
    } else {
        generateMoves<G, BLACK>();
    }
}

template <MoveGenMode T>
template <MoveGenMode G, Color C>
void MoveGenerator<T>::generateMoves() {
    constexpr Color enemy = ~C;
    Square kingSq         = board.kingSq(C);
    U64 occupancy         = board.occupancy();
    U64 targets;

    // generate piece moves unless in double check
    if (G != MoveGenMode::Evasions || !board.isDoubleCheck()) {
        if constexpr (G == MoveGenMode::All) {
            targets = ~board.pieces<PieceType::All>(C);
        } else if constexpr (G == MoveGenMode::Captures) {
            targets = board.pieces<PieceType::All>(enemy);
        } else if constexpr (G == MoveGenMode::Evasions) {
            Square checker = BB::lsb(board.checkers());
            targets        = BB::between(checker, kingSq) | BB::set(checker);
        } else {
            targets = ~occupancy;
        }

        generatePieceMoves<PieceType::Knight, C>(targets, occupancy);
        generatePieceMoves<PieceType::Bishop, C>(targets, occupancy);
        generatePieceMoves<PieceType::Rook, C>(targets, occupancy);
        generatePieceMoves<PieceType::Queen, C>(targets, occupancy);
        generatePawnMoves<G, C>(targets, occupancy);
    }

    // generate king moves
    U64 moves = BB::moves<PieceType::King>(kingSq) &
                (G != MoveGenMode::Evasions ? targets : ~board.pieces<PieceType::All>(C));
    while (moves) {
        Square to  = BB::lsb(moves);
        moves     &= BB::clear(to);
        add(kingSq, to);
    }

    if ((G == MoveGenMode::All || G == MoveGenMode::Quiets) && board.canCastle(C)) {
        Square from = KingOrigin[C];
        if (canCastleOO(occupancy, C)) {
            add(from, KingDestinationOO[C], MoveType::Castle);
        }
        if (canCastleOOO(occupancy, C)) {
            add(from, KingDestinationOOO[C], MoveType::Castle);
        }
    }
};

template <MoveGenMode T>
template <MoveGenMode G, Color C>
void MoveGenerator<T>::generatePawnMoves(const U64 targets, const U64 occupied) {
    constexpr Color enemy = ~C;
    constexpr U64 rank3   = (C == WHITE) ? BB::rankBB(Rank::R3) : BB::rankBB(Rank::R6);
    constexpr U64 rank7   = (C == WHITE) ? BB::rankBB(Rank::R7) : BB::rankBB(Rank::R2);

    U64 moves, vacancies = ~occupied;
    U64 enemies = board.pieces<PieceType::All>(enemy);

    // if generating evasions, only consider enemies that check our king
    if constexpr (G == MoveGenMode::Evasions) {
        enemies &= targets;
    }

    // Generate pawn promotions
    U64 pawns = board.pieces<PieceType::Pawn>(C) & rank7;
    if (pawns) {
        // pawn push promotions
        moves = BB::pawnMoves<PawnMove::Push, C>(pawns) & vacancies;
        if constexpr (G == MoveGenMode::Evasions) moves &= targets;
        addPawnPromotions<PawnMove::Push, C>(moves);

        // capture promotions
        moves = BB::pawnMoves<PawnMove::Left, C>(pawns) & enemies;
        addPawnPromotions<PawnMove::Left, C>(moves);
        moves = BB::pawnMoves<PawnMove::Right, C>(pawns) & enemies;
        addPawnPromotions<PawnMove::Right, C>(moves);
    }

    // Generate captures / enpassant
    pawns = board.pieces<PieceType::Pawn>(C) & ~rank7;
    if constexpr (G != MoveGenMode::Quiets) {
        moves = BB::pawnMoves<PawnMove::Left, C>(pawns) & enemies;
        addPawnMoves<PawnMove::Left, C>(moves);
        moves = BB::pawnMoves<PawnMove::Right, C>(pawns) & enemies;
        addPawnMoves<PawnMove::Right, C>(moves);

        Square enpassant = board.enPassantSq();
        if (enpassant != INVALID) {
            // Only necessary if enemy pawn is targeted, or if in check
            Square enemyPawn = pawnMove<C, PawnMove::Push, false>(enpassant);
            if (G != MoveGenMode::Evasions || (targets & BB::set(enemyPawn))) {
                // Append en passant captures to move list
                addEnPassants<PawnMove::Left, C>(pawns, enpassant);
                addEnPassants<PawnMove::Right, C>(pawns, enpassant);
            }
        }
    }

    // Generate regular pawn pushes
    if constexpr (G != MoveGenMode::Captures) {
        moves               = BB::pawnMoves<PawnMove::Push, C>(pawns) & vacancies;
        U64 doubleMovePawns = moves & rank3,
            doubleMoves     = BB::pawnMoves<PawnMove::Push, C>(doubleMovePawns) & vacancies;

        // If in check, only make moves that block
        if (G == MoveGenMode::Evasions) {
            doubleMoves &= targets;
            moves       &= targets;
        }

        addPawnMoves<PawnMove::Double, C>(doubleMoves);
        addPawnMoves<PawnMove::Push, C>(moves);
    }
}

template <MoveGenMode T>
template <PieceType p, Color c>
void MoveGenerator<T>::generatePieceMoves(const U64 targets, const U64 occupied) {
    U64 bitboard = board.pieces<p>(c);

    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square from  = BB::selectSquare<c>(bitboard);
        bitboard    &= BB::clear(from);

        U64 pieceMoves = BB::moves<p>(from, occupied) & targets;
        while (pieceMoves) {
            Square to   = BB::selectSquare<c>(pieceMoves);
            pieceMoves &= BB::clear(to);

            add(from, to);
        }
    }
}

template <MoveGenMode T>
template <PawnMove p, Color c>
inline void MoveGenerator<T>::addPawnMoves(U64 bitboard) {
    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square to  = BB::selectSquare<c>(bitboard);
        bitboard  &= BB::clear(to);

        // Reverse the move to get the origin square and append move
        Square from = pawnMove<c, p, false>(to);
        add(from, to);
    }
};

template <MoveGenMode T>
template <PawnMove p, Color c>
inline void MoveGenerator<T>::addPawnPromotions(U64 bitboard) {
    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square to  = BB::selectSquare<c>(bitboard);
        bitboard  &= BB::clear(to);

        // Reverse the move to get the origin square and append moves
        Square from = pawnMove<c, p, false>(to);

        add(from, to, MoveType::Promotion, PieceType::Queen);
        add(from, to, MoveType::Promotion, PieceType::Rook);
        add(from, to, MoveType::Promotion, PieceType::Bishop);
        add(from, to, MoveType::Promotion, PieceType::Knight);
    }
}

template <MoveGenMode T>
template <PawnMove p, Color c>
inline void MoveGenerator<T>::addEnPassants(U64 pawns, Square enpassant) {
    U64 bitboard = BB::pawnMoves<p, c>(pawns) & BB::set(enpassant);

    if (bitboard) {
        Square from = pawnMove<c, p, false>(enpassant);
        add(from, enpassant, MoveType::EnPassant);
    }
}

template <MoveGenMode T>
inline bool MoveGenerator<T>::canCastleOO(U64 occupied, Color turn) {
    return (board.canCastleOO(turn)              // castling rights
            && !(occupied & CastlePathOO[turn])  // castle path unoccupied/attacked
            && !board.attacksTo(KingCastlePathOO[turn], ~turn));
}

template <MoveGenMode T>
inline bool MoveGenerator<T>::canCastleOOO(U64 occupied, Color turn) {
    return (board.canCastleOOO(turn)              // castling rights
            && !(occupied & CastlePathOOO[turn])  // castle path unoccupied/attacked
            && !board.attacksTo(KingCastlePathOOO[turn], ~turn));
}
