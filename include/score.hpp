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

constexpr Score PAWN_SCORE   = {PAWN_VALUE_MG, PAWN_VALUE_EG};
constexpr Score KNIGHT_SCORE = {KNIGHT_VALUE_MG, KNIGHT_VALUE_EG};
constexpr Score BISHOP_SCORE = {BISHOP_VALUE_MG, BISHOP_VALUE_EG};
constexpr Score ROOK_SCORE   = {ROOK_VALUE_MG, ROOK_VALUE_EG};
constexpr Score QUEEN_SCORE  = {QUEEN_VALUE_MG, QUEEN_VALUE_EG};

constexpr Score PIECE_SCORE[] = {PAWN_SCORE, KNIGHT_SCORE, BISHOP_SCORE, ROOK_SCORE, QUEEN_SCORE};
constexpr Score pieceScore(PieceType pt) { return PIECE_SCORE[pt - 1]; }

constexpr Score PIECE_SCORES[2][6] = {
    {-PAWN_SCORE, -KNIGHT_SCORE, -BISHOP_SCORE, -ROOK_SCORE, -QUEEN_SCORE, {0, 0}},
    {PAWN_SCORE, KNIGHT_SCORE, BISHOP_SCORE, ROOK_SCORE, QUEEN_SCORE, {0, 0}}};
constexpr Score pieceScore(PieceType pt, Color c) { return PIECE_SCORES[c][pt - 1]; }

const Score ISO_PAWN_PENALTY            = {-5, -15};
const Score BACKWARD_PAWN_PENALTY       = {-9, -25};
const Score DOUBLED_PAWN_PENALTY        = {-11, -56};
const Score REACHABLE_OUTPOST_BONUS     = {31, 22};
const Score BISHOP_OUTPOST_BONUS        = {30, 23};
const Score KNIGHT_OUTPOST_BONUS        = {56, 36};
const Score OUTPOST_BONUS[2]            = {BISHOP_OUTPOST_BONUS, KNIGHT_OUTPOST_BONUS};
const Score MINOR_BEHIND_PAWN_BONUS     = {18, 3};
const Score BISHOP_LONG_DIAG_BONUS      = {40, 0};
const Score BISHOP_PAIR_BONUS           = {50, 80};
const Score BISHOP_PAWN_BLOCKER_PENALTY = {-3, -7};
const Score ROOK_FULL_OPEN_FILE_BONUS   = {40, 20};
const Score ROOK_SEMI_OPEN_FILE_BONUS   = {20, 10};
const Score ROOK_OPEN_FILE_BONUS[2]     = {ROOK_SEMI_OPEN_FILE_BONUS, ROOK_FULL_OPEN_FILE_BONUS};
const Score ROOK_CLOSED_FILE_PENALTY    = {-10, -5};
const Score DISCOVERED_ATTACK_ON_QUEEN_PENALTY = {-50, -25};

// Bonus for friendly pawn at each rank. Index 0 when there is no pawn.
constexpr Score PAWN_SHELTER_BONUS[] = {
    {-30, 0}, {60, 0}, {35, 0}, {-20, 0}, {-5, 0}, {-20, 0}, {-80, 0}};

// Penalty for unblocked enemy pawns at each rank. Index 0 when there is no pawn.
// No end game values bc unblocked pawns evaluated dynamically through search
constexpr Score PAWN_STORM_PENALTY[] = {
    {0, 0}, {-20, 0}, {-120, 0}, {-60, 0}, {-45, 0}, {-20, 0}, {-10, 0}};

// Penalty for blocked enemy pawns at each rank. Index 0 when there is no pawn.
constexpr Score BLOCKED_STORM_PENALTY[] = {
    {0, 0}, {0, 0}, {-60, -60}, {0, -20}, {5, -15}, {10, -10}, {15, -5}};

// KING_OPEN_FILE_BONUS[friendly open file][enemy open file] bonus for king located on a
// closed, semi-open, or open file
constexpr Score KING_OPEN_FILE_BONUS[2][2] = {
    {{20, -10}, {10, 5}},
    {{0, 0}, {-10, 5}},
};

constexpr Score KING_FILE_BONUS[] = {
    {20, 0}, {5, 0}, {-15, 0}, {-30, 0}, {-30, 0}, {-15, 0}, {5, 0}, {20, 0}};

constexpr Score KING_DANGER[] = {
    {0, 0},
    {-12, -6},   // pawn
    {-6, -3},    // knight
    {-8, -4},    // bishop
    {-16, -10},  // rook
    {-24, -12},  // queen
    {0, -10},    // king
};

constexpr Score WEAK_PIECE[] = {
    {0, 0},
    {0, 0},       // pawn
    {-20, -10},   // knight
    {-25, -15},   // bishop
    {-50, -25},   // rook
    {-100, -50},  // queen
    {0, 0},       // king
};

// clang-format off
constexpr Score KNIGHT_MOBILITY[] = {
    {-40, -48}, {-32, -36}, {-8, -20}, {-2, -12}, {2, 6},
    {8, 8},     {12, 12},   {16, 16},  {24, 16}
};
constexpr Score BISHOP_MOBILITY[] = {
    {-32, -40}, {-16, -16}, {8, -4},   {16, 8},   {24, 16},
    {32, 24},   {32, 36},   {40, 36},  {40, 40},  {44, 48},
    {48, 48},   {56, 56},   {56, 56},  {64, 64}
};
constexpr Score ROOK_MOBILITY[] = {
    {-40, -56}, {-16, -8}, {0, 12},   {0, 28},   {4, 44},
    {8, 64},    {12, 64},  {20, 80},  {28, 88},  {28, 88},
    {28, 96},   {32, 104}, {36, 108}, {40, 112}, {44, 120}
};
constexpr Score QUEEN_MOBILITY[] = {
    {-20, -32}, {-12, -20}, {-4, -4},   {-4, 12},   {12, 24},  {16, 36},  {16, 40},
    {24, 48},   {28, 48},   {36, 60},   {40, 60},   {44, 64},  {44, 80},  {48, 80},
    {48, 88},   {48, 88},   {48, 88},   {48, 92},   {52, 96},  {56, 96},  {60, 100},
    {68, 108},  {68, 112},  {68, 112},  {72, 116},  {72, 120}, {76, 124}, {80, 140}
};
// clang-format on

constexpr const Score* MOBILITY_BONUS[] = {
    nullptr,
    nullptr,
    KNIGHT_MOBILITY,
    BISHOP_MOBILITY,
    ROOK_MOBILITY,
    QUEEN_MOBILITY,
};

constexpr Score pieceSqScore(PieceType pt, Color c, Square sq) {
    sq = SQUARE_MAP[c][sq];
    Score score{PSQ_VALUES[pt - 1][MIDGAME][sq], PSQ_VALUES[pt - 1][ENDGAME][sq]};
    return (score * c * 2) - score;
}
