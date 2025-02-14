#ifndef LATRUNCULI_SCORE_H
#define LATRUNCULI_SCORE_H

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
const Score BISHOP_PAWN_BLOCKER_PENALTY = {-3, -7};
const Score ROOK_ON_QUEEN_FILE_BONUS = {6, 11};

#endif
