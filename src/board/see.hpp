#pragma once

#include <array>

#include "core/piece.hpp"

namespace see {

// Fixed exchange prices for threshold SEE, independent of evaluation weights.
// Numerical Board::see and capture-evasion ordering retain evaluation prices.
inline constexpr std::array<int, N_PIECETYPES> piece_values{0, 100, 350, 385, 655, 1220, 0};

inline constexpr int piece_value(PieceType piece) {
    return piece_values[piece];
}

} // namespace see
