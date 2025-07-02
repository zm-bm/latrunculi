#pragma once

#include "constants.hpp"
#include "score.hpp"
#include "types.hpp"

constexpr int pieceValue(PieceType pieceType) { return PIECE_VALUES[idx(pieceType)]; }

constexpr bool isMateScore(int score) { return std::abs(score) > MATE_IN_MAX_PLY; }

constexpr int mateDistance(int score) { return MATE_SCORE - std::abs(score); }

constexpr int ttScore(int score, int ply) {
    if (score >= MATE_IN_MAX_PLY)
        score += ply;
    else if (score <= MATE_IN_MAX_PLY)
        score -= ply;
    return score;
}

constexpr Score pieceScore(PieceType pt) {
    auto p = idx(pt);
    return PIECE_SCORES[WHITE][p];
}

constexpr Score pieceScore(PieceType pt, Color c) {
    auto p = idx(pt);
    return PIECE_SCORES[c][p];
}

constexpr Score pieceSqScore(PieceType pt, Color c, Square sq) {
    auto p = idx(pt) - 1;
    sq     = SQUARE_MAP[c][sq];
    Score score{PSQ_VALUES[p][idx(Phase::MidGame)][sq], PSQ_VALUES[p][idx(Phase::EndGame)][sq]};
    return (score * c * 2) - score;
}
