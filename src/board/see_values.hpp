#pragma once

#include <array>

#include "core/piece.hpp"

namespace see {

// Fixed exchange prices (pawn = 100), shared by SEE and capture ordering.
using Values = std::array<int, N_PIECETYPES>;
inline constexpr Values piece_values{0, 100, 350, 385, 655, 1220, 0};

inline constexpr int piece_value(PieceType piece, const Values& values = piece_values) {
    return values[piece];
}

} // namespace see
