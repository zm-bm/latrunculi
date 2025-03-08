#pragma once

#include <array>
#include <iostream>

#include "bb.hpp"
#include "chess.hpp"
#include "magics.hpp"
#include "move.hpp"
#include "types.hpp"

template <GenType T>
class MoveGenerator {
   public:
    MoveGenerator(Chess* chess);

    const Move* begin() { return moves.begin(); }
    const Move* end() { return last; }
    bool empty() { return last == moves.data(); }
    size_t size() { return static_cast<std::size_t>(last - moves.data()); }

    inline void add(Square from,
                    Square to,
                    MoveType mtype = NORMAL,
                    PieceType promoPiece = KNIGHT) {
        new (last++) Move(from, to, mtype, promoPiece);
    }

   private:
    Chess* chess;
    std::array<Move, MAX_MOVES> moves;
    Move* last;

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

    bool canCastleOO(U64 occ, Color turn);
    bool canCastleOOO(U64 occ, Color turn);
};

template <GenType T>
MoveGenerator<T>::MoveGenerator(Chess* chess) : chess(chess), last(moves.data()) {
    if (chess->isCheck()) {
        generate<GenType::Evasions>();
    } else {
        generate<T>();
    }
}

template <GenType T>
template <GenType G>
void MoveGenerator<T>::generate() {
    if (chess->turn == WHITE) {
        generateMoves<G, WHITE>();
    } else {
        generateMoves<G, BLACK>();
    }

    for (Move* move = moves.data(); move != last; ++move) {
        int score = 0;
        auto fromPiece = pieceTypeOf(chess->board.pieceOn(move->from()));
        auto toPiece = pieceTypeOf(chess->board.pieceOn(move->to()));
        if (toPiece != NO_PIECE_TYPE) {
            move->score += 10 * PIECE_VALUES[MIDGAME][WHITE][toPiece] -
                         PIECE_VALUES[MIDGAME][WHITE][fromPiece];
        }
        auto moveType = move->type();
        auto promoPiece = move->promoPiece();
        if (moveType == PROMOTION) {
            move->score += PIECE_VALUES[MIDGAME][WHITE][promoPiece];
        }
    }

    auto comp = [](const Move& a, const Move& b) { return a.score > b.score; };
    std::stable_sort(moves.begin(), last, comp);
}

template <GenType T>
template <GenType G, Color C>
void MoveGenerator<T>::generateMoves() {
    constexpr Color enemy = ~C;
    Square kingSq = chess->board.kingSq(C);
    U64 occupancy = chess->board.occupancy();
    U64 targets;

    // generate piece moves unless in
    if (G != GenType::Evasions || !chess->isDoubleCheck()) {
        if constexpr (G == GenType::Legal) {
            targets = ~chess->board.pieces<ALL_PIECES>(C);
        } else if constexpr (G == GenType::Captures) {
            targets = chess->board.pieces<ALL_PIECES>(enemy);
        } else if constexpr (G == GenType::Evasions) {
            Square checker = BB::lsb(chess->getCheckingPieces());
            targets = BB::betweenBB(checker, kingSq) | BB::set(checker);
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
    U64 moves = BB::pieceMoves<KING>(kingSq) &
                (G != GenType::Evasions ? targets : ~chess->board.pieces<ALL_PIECES>(C));
    while (moves) {
        Square to = BB::lsb(moves);
        moves &= BB::clear(to);
        add(kingSq, to);
    }

    if ((G == GenType::Legal || G == GenType::Quiets) && chess->state.at(chess->ply).canCastle(C)) {
        Square from = KingOrigin[C];
        if (canCastleOO(occupancy, C)) add(from, KingDestinationOO[C], CASTLE);
        if (canCastleOOO(occupancy, C)) add(from, KingDestinationOOO[C], CASTLE);
    }
};

template <GenType T>
template <GenType G, Color C>
void MoveGenerator<T>::generatePawnMoves(const U64 targets, const U64 occ) {
    constexpr Color enemy = ~C;
    constexpr U64 rank3 = (C == WHITE) ? BB::rank(RANK3) : BB::rank(RANK6);
    constexpr U64 rank7 = (C == WHITE) ? BB::rank(RANK7) : BB::rank(RANK2);

    U64 moves, vacancies = ~occ;
    U64 enemies = chess->board.pieces<ALL_PIECES>(enemy);

    // if generating evasions, only consider enemies that check our king
    if constexpr (G == GenType::Evasions) {
        enemies &= targets;
    }

    // Generate pawn promotions
    U64 pawns = chess->board.pieces<PAWN>(C) & rank7;
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
    pawns = chess->board.pieces<PAWN>(C) & ~rank7;
    if constexpr (G != GenType::Quiets) {
        moves = BB::pawnMoves<LEFT, C>(pawns) & enemies;
        addPawnMoves<LEFT, C>(moves);
        moves = BB::pawnMoves<RIGHT, C>(pawns) & enemies;
        addPawnMoves<RIGHT, C>(moves);

        Square enpassant = chess->getEnPassant();
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
        moves = BB::pawnMoves<PUSH, C>(pawns) & vacancies;
        U64 doubleMovePawns = moves & rank3,
            doubleMoves = BB::pawnMoves<PUSH, C>(doubleMovePawns) & vacancies;

        // If in check, only make moves that block
        if (G == GenType::Evasions) {
            doubleMoves &= targets;
            moves &= targets;
        }

        addPawnMoves<DOUBLE, C>(doubleMoves);
        addPawnMoves<PUSH, C>(moves);
    }
}

template <GenType T>
template <PieceType p, Color c>
void MoveGenerator<T>::generatePieceMoves(const U64 targets, const U64 occ) {
    Color turn = chess->turn;
    U64 bitboard = chess->board.pieces<p>(turn);

    while (bitboard) {
        // Pop lsb bit and clear it from the bitboard
        Square from = BB::advancedSq<c>(bitboard);
        bitboard &= BB::clear(from);

        U64 pieceMoves = BB::pieceMoves<p>(from, occ) & targets;
        while (pieceMoves) {
            Square to = BB::advancedSq<c>(pieceMoves);
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
        Square to = BB::advancedSq<c>(bitboard);
        bitboard &= BB::clear(to);

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
        Square to = BB::advancedSq<c>(bitboard);
        bitboard &= BB::clear(to);

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
inline bool MoveGenerator<T>::canCastleOO(U64 occ, Color turn) {
    return (chess->state.at(chess->ply).canCastleOO(turn)  // castling rights
            && !(occ & CastlePathOO[turn])             // castle path unoccupied/attacked
            && !chess->board.isBitboardAttacked(KingCastlePathOO[turn], ~turn));
}

template <GenType T>
inline bool MoveGenerator<T>::canCastleOOO(U64 occ, Color turn) {
    return (chess->state.at(chess->ply).canCastleOOO(turn)  // castling rights
            && !(occ & CastlePathOOO[turn])             // castle path unoccupied/attacked
            && !chess->board.isBitboardAttacked(KingCastlePathOOO[turn], ~turn));
}
