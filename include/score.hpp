#pragma once

#include <iomanip>

#include "constants.hpp"

struct Score {
    int mg = 0;
    int eg = 0;

    int taper(int phase) const {
        int egPhase = PHASE_LIMIT - phase;
        return ((mg * phase) + (eg * egPhase)) / PHASE_LIMIT;
    }

    // operators
    Score operator+(const Score& other) const { return Score{mg + other.mg, eg + other.eg}; }
    Score operator-(const Score& other) const { return Score{mg - other.mg, eg - other.eg}; }
    Score operator*(int scalar) const { return Score{mg * scalar, eg * scalar}; }
    Score operator-() const { return Score{-mg, -eg}; }
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

    friend std::ostream& operator<<(std::ostream& os, const Score& score) {
        os << std::setw(5) << double(score.mg) / PAWN_VALUE_MG << " ";
        os << std::setw(5) << double(score.eg) / PAWN_VALUE_MG;
        return os;
    }
};

const Score ISO_PAWN_PENALTY = {-5, -15};
const Score BACKWARD_PAWN_PENALTY = {-9, -25};
const Score DOUBLED_PAWN_PENALTY = {-11, -56};
const Score REACHABLE_OUTPOST_BONUS = {31, 22};
const Score BISHOP_OUTPOST_BONUS = {30, 23};
const Score KNIGHT_OUTPOST_BONUS = {56, 36};
const Score OUTPOST_BONUS[2] = {BISHOP_OUTPOST_BONUS, KNIGHT_OUTPOST_BONUS};
const Score MINOR_BEHIND_PAWN_BONUS = {18, 3};
const Score BISHOP_LONG_DIAG_BONUS = {40, 0};
const Score BISHOP_PAIR_BONUS = {50, 80};
const Score BISHOP_PAWN_BLOCKER_PENALTY = {-3, -7};
const Score ROOK_FULL_OPEN_FILE_BONUS = Score{40, 20};
const Score ROOK_SEMI_OPEN_FILE_BONUS = Score{20, 10};
const Score ROOK_OPEN_FILE_BONUS[2] = {ROOK_SEMI_OPEN_FILE_BONUS, ROOK_FULL_OPEN_FILE_BONUS};
const Score ROOK_CLOSED_FILE_PENALTY = {-10, -5};
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
    {0, -10},   // king
};

// clang-format off
constexpr Score KNIGHT_MOBILITY[] = {
    {-50, -60}, {-40, -45}, {-10, -25}, {-2, -14}, {2, 6},
    {10, 10},   {15, 15},   {20, 18},   {30, 20}
};
constexpr Score BISHOP_MOBILITY[] = {
    {-40, -50}, {-20, -20}, {10, -5},   {20, 10},   {30, 20},
    {40, 30},   {40, 45},   {50, 45},   {50, 50},   {55, 60},
    {60, 60},   {70, 70},   {70, 70},   {80, 80}
};
constexpr Score ROOK_MOBILITY[] = {
    {-50, -70}, {-20, -10}, {0, 15},    {0, 35},    {5, 55},
    {10, 80},   {15, 80},   {25, 100},  {35, 110},  {35, 110},
    {35, 120},  {40, 130},  {45, 135},  {50, 140},  {55, 150}
};
constexpr Score QUEEN_MOBILITY[] = {
    {-25, -40}, {-15, -25}, {-5, -5},   {-5, 15},   {15, 30},   {20, 45},   {20, 50},
    {30, 60},   {35, 60},   {45, 75},   {50, 75},   {55, 80},   {55, 100},  {60, 100},
    {60, 110},  {60, 110},  {60, 110},  {60, 115},  {65, 120},  {70, 120},  {75, 125},
    {85, 135},  {85, 140},  {85, 140},  {90, 145},  {90, 150},  {95, 155},  {100, 175}
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
