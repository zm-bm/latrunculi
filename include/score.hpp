#pragma once

#include <iomanip>

#include "constants.hpp"

struct Score {
    int mg = 0;
    int eg = 0;

    // operators
    constexpr Score operator+(const Score& other) const { return {mg + other.mg, eg + other.eg}; }
    constexpr Score operator-(const Score& other) const { return {mg - other.mg, eg - other.eg}; }
    constexpr Score operator*(int scalar) const { return {mg * scalar, eg * scalar}; }
    constexpr Score operator-() const { return {-mg, -eg}; }
    bool operator==(const Score& other) const { return mg == other.mg && eg == other.eg; }
    bool operator!=(const Score& other) const { return !(*this == other); }
    bool operator<(const Score& other) const { return mg < other.mg; }

    Score& operator+=(const Score& other) {
        mg += other.mg;
        eg += other.eg;
        return *this;
    }

    Score& operator-=(const Score& other) {
        mg -= other.mg;
        eg -= other.eg;
        return *this;
    }

    Score& operator*=(int scalar) {
        mg *= scalar;
        eg *= scalar;
        return *this;
    }

    int taper(int phase) const {
        int egPhase = PHASE_LIMIT - phase;
        return ((mg * phase) + (eg * egPhase)) / PHASE_LIMIT;
    }

    friend std::ostream& operator<<(std::ostream& os, const Score& score) {
        os << std::setw(5) << double(score.mg) / PAWN_VALUE_MG << " ";
        os << std::setw(5) << double(score.eg) / PAWN_VALUE_MG;
        return os;
    }
};

constexpr Score ZERO_SCORE   = {0, 0};
constexpr Score PAWN_SCORE   = {PAWN_VALUE_MG, PAWN_VALUE_EG};
constexpr Score KNIGHT_SCORE = {KNIGHT_VALUE_MG, KNIGHT_VALUE_EG};
constexpr Score BISHOP_SCORE = {BISHOP_VALUE_MG, BISHOP_VALUE_EG};
constexpr Score ROOK_SCORE   = {ROOK_VALUE_MG, ROOK_VALUE_EG};
constexpr Score QUEEN_SCORE  = {QUEEN_VALUE_MG, QUEEN_VALUE_EG};

constexpr Score PIECE_SCORES[N_COLORS][N_PIECES] = {
    {ZERO_SCORE, -PAWN_SCORE, -KNIGHT_SCORE, -BISHOP_SCORE, -ROOK_SCORE, -QUEEN_SCORE, ZERO_SCORE},
    {ZERO_SCORE, PAWN_SCORE, KNIGHT_SCORE, BISHOP_SCORE, ROOK_SCORE, QUEEN_SCORE, ZERO_SCORE}};

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
