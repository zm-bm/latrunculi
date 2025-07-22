#pragma once

#include "constants.hpp"
#include "score.hpp"
#include "types.hpp"

// --------------------------
// Piece values and scores
// --------------------------

constexpr int pieceValue(PieceType pieceType) { return PIECE_VALUES[idx(pieceType)]; }

constexpr Score pieceScore(PieceType pt) {
    auto p = idx(pt);
    return PIECE_SCORES[WHITE][p];
}

constexpr Score pieceScore(PieceType pt, Color c) {
    auto p = idx(pt);
    return PIECE_SCORES[c][p];
}

constexpr Score pieceSqScore(PieceType pt, Color c, Square sq) {
    auto p  = idx(pt) - 1;
    auto mg = idx(Phase::MidGame);
    auto eg = idx(Phase::EndGame);
    sq      = SQUARE_MAP[c][sq];
    Score score{PSQ_VALUES[p][mg][sq], PSQ_VALUES[p][eg][sq]};
    return (score * c * 2) - score;
}

// --------------------------
// Mate helpers
// --------------------------

constexpr bool isMate(int value) { return std::abs(value) > MATE_BOUND; }

constexpr int mateDistance(int value) { return MATE_VALUE - std::abs(value); }

// -----------------
// Pawn move helpers
// -----------------

template <Color c, PawnMove p, bool forward>
inline Square pawnMove(const Square sq) {
    return (forward == (c == WHITE)) ? Square(sq + static_cast<int>(p))
                                     : Square(sq - static_cast<int>(p));
}

template <PawnMove p, bool forward>
inline Square pawnMove(const Square sq, const Color c) {
    return (c == WHITE) ? pawnMove<WHITE, p, forward>(sq) : pawnMove<BLACK, p, forward>(sq);
}
