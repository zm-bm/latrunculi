#pragma once

#include <array>

#include "core/piece.hpp"

namespace material {

// Search prices (pawn = 100) for SEE, capture ordering, and qsearch estimates.
// These are heuristics, independent of NNUE's positional evaluation.
using Values = std::array<int, N_PIECETYPES>;
inline constexpr Values piece_values{0, 100, 360, 385, 655, 1235, 0};

inline constexpr int piece_value(PieceType piece, const Values& values = piece_values) noexcept {
    return values[piece];
}

} // namespace material
