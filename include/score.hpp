#pragma once

#include <iomanip>

struct Score {
    int mg = 0;
    int eg = 0;

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

    friend std::ostream& operator<<(std::ostream& os, const Score& score);
};
