#pragma once

#include "core/types.hpp"

struct TaperedScore {
    EvalValue mg = 0;
    EvalValue eg = 0;

    static TaperedScore const Zero;

    constexpr TaperedScore operator+(const TaperedScore& other) const;
    constexpr TaperedScore operator-(const TaperedScore& other) const;
    constexpr TaperedScore operator*(EvalValue scalar) const;
    constexpr TaperedScore operator-() const;
    constexpr bool         operator==(const TaperedScore& other) const;
    constexpr bool         operator!=(const TaperedScore& other) const;
    constexpr bool         operator<(const TaperedScore& other) const;

    TaperedScore& operator+=(const TaperedScore& other);
    TaperedScore& operator-=(const TaperedScore& other);
    TaperedScore& operator*=(EvalValue scalar);
};

constexpr TaperedScore TaperedScore::Zero = {0, 0};

constexpr TaperedScore TaperedScore::operator+(const TaperedScore& other) const {
    return {mg + other.mg, eg + other.eg};
}

constexpr TaperedScore TaperedScore::operator-(const TaperedScore& other) const {
    return {mg - other.mg, eg - other.eg};
}

constexpr TaperedScore TaperedScore::operator*(EvalValue scalar) const {
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

constexpr bool TaperedScore::operator<(const TaperedScore& other) const {
    return mg < other.mg;
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

inline TaperedScore& TaperedScore::operator*=(EvalValue scalar) {
    mg *= scalar;
    eg *= scalar;
    return *this;
}
