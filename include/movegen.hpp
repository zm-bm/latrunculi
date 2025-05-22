#pragma once

#include <array>
#include <iostream>
#include <optional>

#include "bb.hpp"
#include "board.hpp"
#include "eval.hpp"
#include "magics.hpp"
#include "move.hpp"
#include "move_order.hpp"
#include "thread.hpp"
#include "tt.hpp"
#include "types.hpp"

template <GenType T>
class MoveGenerator {
   public:
    MoveGenerator(const Board& board);

    void sort(MoveOrder);

    Move& operator[](int index) { return moves[index]; }
    const Move* begin() { return moves.begin(); }
    const Move* end() { return last; }
    const bool empty() { return last == moves.data(); }
    const size_t size() { return static_cast<std::size_t>(last - moves.data()); }

   private:
    const Board& board;
    std::array<Move, MAX_MOVES> moves;
    Move* last;

    void add(Square, Square, MoveType = NORMAL, PieceType = KNIGHT);

    template <GenType>
    void generate();
    template <GenType, Color>
    void generateMoves();
    template <GenType, Color>
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

template <GenType T>
MoveGenerator<T>::MoveGenerator(const Board& board) : board(board), last(moves.data()) {
    if (board.isCheck()) {
        generate<GenType::Evasions>();
    } else {
        generate<T>();
    }
}

template <GenType T>
inline void MoveGenerator<T>::add(Square from, Square to, MoveType mtype, PieceType promoPiece) {
    new (last++) Move(from, to, mtype, promoPiece);
}

template <GenType T>
inline void MoveGenerator<T>::sort(MoveOrder priority) {
    for (Move* move = moves.data(); move != last; ++move) {
        move->priority = priority.scoreMove(*move);
    }

    auto comp = [](const Move& a, const Move& b) { return a.priority > b.priority; };
    std::stable_sort(moves.begin(), last, comp);
}

template <GenType T>
template <GenType G>
void MoveGenerator<T>::generate() {
    if (board.sideToMove() == WHITE) {
        generateMoves<G, WHITE>();
    } else {
        generateMoves<G, BLACK>();
    }
}

template <GenType T>
template <GenType G, Color C>
void MoveGenerator<T>::generateMoves() {
    constexpr Color enemy = ~C;
    Square kingSq         = board.kingSq(C);
    U64 occupancy         = board.occupancy();
    U64 targets;

    // generate piece moves unless in double check
    if (G != GenType::Evasions || !board.isDoubleCheck()) {
        if constexpr (G == GenType::All) {
            targets = ~board.pieces<ALL_PIECES>(C);
        } else if constexpr (G == GenType::Captures) {
            targets = board.pieces<ALL_PIECES>(enemy);
        } else if constexpr (G == GenType::Evasions) {
            Square checker = BB::lsb(board.checkers());
            targets        = BB::betweenBB(checker, kingSq) | BB::set(checker);
        } else {
            targets = ~occupancy;
        }

        generatePieceMoves<KNIGHT, C>(targets, occupancy);
        generatePieceMoves<BISHOP, C>(targets, occupancy);
        generatePieceMoves<ROOK, C>(targets, occupancy);
        generatePieceMoves<QUEEN, C>(targets, occupancy);
        generatePawnMoves<G, C>(targets, occupancy);
    }

    // generate king moves
    U64 moves =
        BB::moves<KING>(kingSq) & (G != GenType::Evasions ? targets : ~board.pieces<ALL_PIECES>(C));
    while (moves) {
        Square to  = BB::lsb(moves);
        moves     &= BB::clear(to);
        add(kingSq, to);
    }

    if ((G == GenType::All || G == GenType::Quiets) && board.canCastle(C)) {
        Square from = KingOrigin[C];
        if (canCastleOO(occupancy, C)) add(from, KingDestinationOO[C], CASTLE);
        if (canCastleOOO(occupancy, C)) add(from, KingDestinationOOO[C], CASTLE);
    }
};

template <GenType T>
template <GenType G, Color C>
void MoveGenerator<T>::generatePawnMoves(const U64 targets, const U64 occupied) {
    constexpr Color enemy = ~C;
    constexpr U64 rank3   = (C == WHITE) ? BB::rank(RANK3) : BB::rank(RANK6);
    constexpr U64 rank7   = (C == WHITE) ? BB::rank(RANK7) : BB::rank(RANK2);

    U64 moves, vacancies = ~occupied;
    U64 enemies = board.pieces<ALL_PIECES>(enemy);

    // if generating evasions, only consider enemies that check our king
    if constexpr (G == GenType::Evasions) {
        enemies &= targets;
    }

    // Generate pawn promotions
    U64 pawns = board.pieces<PAWN>(C) & rank7;
    if (pawns) {
        // pawn push promotions
        moves = BB::pawnMoves<PUSH, C>(pawns) & vacancies;
        if constexpr (G == GenType::Evasions) moves &= targets;
        addPawnPromotions<PUSH, C>(moves);

        // capture promotions
        moves = BB::pawnMoves<LEFT, C>(pawns) & enemies;
        addPawnPromotions<LEFT, C>(moves);
        moves = BB::pawnMoves<RIGHT, C>(pawns) & enemies;
        addPawnPromotions<RIGHT, C>(moves);
    }

    // Generate captures / enpassant
    pawns = board.pieces<PAWN>(C) & ~rank7;
    if constexpr (G != GenType::Quiets) {
        moves = BB::pawnMoves<LEFT, C>(pawns) & enemies;
        addPawnMoves<LEFT, C>(moves);
        moves = BB::pawnMoves<RIGHT, C>(pawns) & enemies;
        addPawnMoves<RIGHT, C>(moves);

        Square enpassant = board.enPassantSq();
        if (enpassant != INVALID) {
            // Only necessary if enemy pawn is targeted, or if in check
            Square enemyPawn = pawnMove<C, PUSH, false>(enpassant);
            if (G != GenType::Evasions || (targets & BB::set(enemyPawn))) {
                // Append en passant captures to move list
                addEnPassants<LEFT, C>(pawns, enpassant);
                addEnPassants<RIGHT, C>(pawns, enpassant);
            }
        }
    }

    // Generate regular pawn pushes
    if constexpr (G != GenType::Captures) {
        moves               = BB::pawnMoves<PUSH, C>(pawns) & vacancies;
        U64 doubleMovePawns = moves & rank3,
            doubleMoves     = BB::pawnMoves<PUSH, C>(doubleMovePawns) & vacancies;

        // If in check, only make moves that block
        if (G == GenType::Evasions) {
            doubleMoves &= targets;
            moves       &= targets;
        }

        addPawnMoves<DOUBLE, C>(doubleMoves);
        addPawnMoves<PUSH, C>(moves);
    }
}

template <GenType T>
template <PieceType p, Color c>
void MoveGenerator<T>::generatePieceMoves(const U64 targets, const U64 occupied) {
    U64 bitboard = board.pieces<p>(c);

    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square from  = BB::advancedSq<c>(bitboard);
        bitboard    &= BB::clear(from);

        U64 pieceMoves = BB::moves<p>(from, occupied) & targets;
        while (pieceMoves) {
            Square to   = BB::advancedSq<c>(pieceMoves);
            pieceMoves &= BB::clear(to);

            add(from, to);
        }
    }
}

template <GenType T>
template <PawnMove p, Color c>
inline void MoveGenerator<T>::addPawnMoves(U64 bitboard) {
    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square to  = BB::advancedSq<c>(bitboard);
        bitboard  &= BB::clear(to);

        // Reverse the move to get the origin square and append move
        Square from = pawnMove<c, p, false>(to);
        add(from, to);
    }
};

template <GenType T>
template <PawnMove p, Color c>
inline void MoveGenerator<T>::addPawnPromotions(U64 bitboard) {
    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square to  = BB::advancedSq<c>(bitboard);
        bitboard  &= BB::clear(to);

        // Reverse the move to get the origin square and append moves
        Square from = pawnMove<c, p, false>(to);

        add(from, to, PROMOTION, QUEEN);
        add(from, to, PROMOTION, ROOK);
        add(from, to, PROMOTION, BISHOP);
        add(from, to, PROMOTION, KNIGHT);
    }
}

template <GenType T>
template <PawnMove p, Color c>
inline void MoveGenerator<T>::addEnPassants(U64 pawns, Square enpassant) {
    U64 bitboard = BB::pawnMoves<p, c>(pawns) & BB::set(enpassant);

    if (bitboard) {
        Square from = pawnMove<c, p, false>(enpassant);
        add(from, enpassant, ENPASSANT);
    }
}

template <GenType T>
inline bool MoveGenerator<T>::canCastleOO(U64 occupied, Color turn) {
    return (board.canCastleOO(turn)              // castling rights
            && !(occupied & CastlePathOO[turn])  // castle path unoccupied/attacked
            && !board.attacksTo(KingCastlePathOO[turn], ~turn));
}

template <GenType T>
inline bool MoveGenerator<T>::canCastleOOO(U64 occupied, Color turn) {
    return (board.canCastleOOO(turn)              // castling rights
            && !(occupied & CastlePathOOO[turn])  // castle path unoccupied/attacked
            && !board.attacksTo(KingCastlePathOOO[turn], ~turn));
}
