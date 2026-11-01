#pragma once

#include <array>

#include "core/piece.hpp"

namespace see {

// Fixed exchange prices (pawn = 100), shared by SEE and capture ordering.
inline constexpr std::array<int, N_PIECETYPES> piece_values{0, 100, 350, 385, 655, 1220, 0};

inline constexpr int piece_value(PieceType piece) {
    return piece_values[piece];
}

} // namespace see
