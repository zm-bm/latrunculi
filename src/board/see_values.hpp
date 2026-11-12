#pragma once

#include <array>

#include "core/piece.hpp"

namespace see {

// Fixed exchange prices (pawn = 100), shared by SEE and capture ordering.
inline constexpr std::array<int, N_PIECETYPES> piece_values{0, 100, 349, 381, 700, 1208, 0};

inline constexpr int piece_value(PieceType piece) {
    return piece_values[piece];
}

} // namespace see
