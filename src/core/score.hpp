#pragma once

struct Score {
    int mg = 0;
    int eg = 0;

    static Score const Zero;

    constexpr Score operator+(const Score& other) const;
    constexpr Score operator-(const Score& other) const;
    constexpr Score operator*(int scalar) const;
    constexpr Score operator-() const;
    constexpr bool  operator==(const Score& other) const;
    constexpr bool  operator!=(const Score& other) const;
    constexpr bool  operator<(const Score& other) const;

    Score& operator+=(const Score& other);
    Score& operator-=(const Score& other);
    Score& operator*=(int scalar);
};

constexpr Score Score::Zero = {0, 0};

constexpr Score Score::operator+(const Score& other) const {
    return {mg + other.mg, eg + other.eg};
}

constexpr Score Score::operator-(const Score& other) const {
    return {mg - other.mg, eg - other.eg};
}

constexpr Score Score::operator*(int scalar) const {
    return {mg * scalar, eg * scalar};
}

constexpr Score Score::operator-() const {
    return {-mg, -eg};
}

constexpr bool Score::operator==(const Score& other) const {
    return mg == other.mg && eg == other.eg;
}

constexpr bool Score::operator!=(const Score& other) const {
    return !(*this == other);
}

constexpr bool Score::operator<(const Score& other) const {
    return mg < other.mg;
}

inline Score& Score::operator+=(const Score& other) {
    mg += other.mg;
    eg += other.eg;
    return *this;
}

inline Score& Score::operator-=(const Score& other) {
    mg -= other.mg;
    eg -= other.eg;
    return *this;
}

inline Score& Score::operator*=(int scalar) {
    mg *= scalar;
    eg *= scalar;
    return *this;
}
