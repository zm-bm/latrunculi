#include "constants.hpp"

struct Score {
    int mg = 0;
    int eg = 0;

    int taper(int phase) {
        int egPhase = PHASE_LIMIT - phase;
        return ((mg * phase) + (eg * egPhase)) / PHASE_LIMIT;
    }

    // operators
    Score operator+(const Score& other) const { return Score{mg + other.mg, eg + other.eg}; }
    Score operator-(const Score& other) const { return Score{mg - other.mg, eg - other.eg}; }
    Score operator*(int scalar) const { return Score{mg * scalar, eg * scalar}; }

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
