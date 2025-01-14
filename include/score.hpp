#ifndef LATRUNCULI_SCORE_H
#define LATRUNCULI_SCORE_H

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
};
#endif
