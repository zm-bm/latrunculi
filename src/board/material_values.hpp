#pragma once

#include <array>

#include "core/piece.hpp"

// Search material estimates retain their historical scale independently of NNUE.
// SEE has its own exchange prices in see_values.hpp.
namespace material {

constexpr Score value(PieceType piece) noexcept {
    constexpr std::array<Score, N_PIECETYPES> values = {0, 100, 602, 613, 880, 2075, 0};
    return values[piece];
}

constexpr Score capture_value(PieceType piece) noexcept {
    constexpr std::array<Score, N_PIECETYPES> values = {0, 202, 679, 709, 1090, 2078, 0};
    return values[piece];
}

} // namespace material
