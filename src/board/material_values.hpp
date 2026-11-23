#pragma once

#include <array>

#include "core/piece.hpp"

// Search-only estimates retain their historical scale independently of NNUE.
// SEE has its own exchange prices in see_values.hpp.
namespace material {

// Used by non-pawn material thresholds and singular-search margins.
constexpr Score pruning_value(PieceType piece) noexcept {
    constexpr std::array<Score, N_PIECETYPES> values = {0, 100, 602, 613, 880, 2075, 0};
    return values[piece];
}

// Used by quiescence capture-delta pruning.
constexpr Score qsearch_capture_value(PieceType piece) noexcept {
    constexpr std::array<Score, N_PIECETYPES> values = {0, 202, 679, 709, 1090, 2078, 0};
    return values[piece];
}

} // namespace material
