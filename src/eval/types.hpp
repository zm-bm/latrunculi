#pragma once

#include <cstdint>

namespace eval {

enum class Term : std::uint8_t {
    Material = 0,
    Squares,
    Pawns,
    Knights,
    Bishops,
    Rooks,
    Queens,
    King,
    Mobility,
    Threats,
    Count,
};

enum class Phase : std::uint8_t {
    Midgame = 0,
    Endgame,
    Count,
};

} // namespace eval
