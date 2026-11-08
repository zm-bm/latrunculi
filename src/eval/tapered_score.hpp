#pragma once

#include "core/types.hpp"

namespace eval {

// Middlegame/endgame pair, before blending to a single Score.
struct TaperedScore {
    Score mg = 0;
    Score eg = 0;

    static TaperedScore const Zero;

    constexpr TaperedScore operator+(const TaperedScore& other) const;
    constexpr TaperedScore operator-(const TaperedScore& other) const;
    constexpr TaperedScore operator*(Score scalar) const;
    constexpr TaperedScore operator-() const;
    constexpr bool operator==(const TaperedScore& other) const;
    constexpr bool operator!=(const TaperedScore& other) const;

    TaperedScore& operator+=(const TaperedScore& other);
    TaperedScore& operator-=(const TaperedScore& other);
    TaperedScore& operator*=(Score scalar);
};

constexpr TaperedScore TaperedScore::Zero = {0, 0};

constexpr TaperedScore TaperedScore::operator+(const TaperedScore& other) const {
    return {mg + other.mg, eg + other.eg};
}

constexpr TaperedScore TaperedScore::operator-(const TaperedScore& other) const {
    return {mg - other.mg, eg - other.eg};
}

constexpr TaperedScore TaperedScore::operator*(Score scalar) const {
    return {mg * scalar, eg * scalar};
}

constexpr TaperedScore TaperedScore::operator-() const {
    return {-mg, -eg};
}

constexpr bool TaperedScore::operator==(const TaperedScore& other) const {
    return mg == other.mg && eg == other.eg;
}

constexpr bool TaperedScore::operator!=(const TaperedScore& other) const {
    return !(*this == other);
}

inline TaperedScore& TaperedScore::operator+=(const TaperedScore& other) {
    mg += other.mg;
    eg += other.eg;
    return *this;
}

inline TaperedScore& TaperedScore::operator-=(const TaperedScore& other) {
    mg -= other.mg;
    eg -= other.eg;
    return *this;
}

inline TaperedScore& TaperedScore::operator*=(Score scalar) {
    mg *= scalar;
    eg *= scalar;
    return *this;
}

} // namespace eval
