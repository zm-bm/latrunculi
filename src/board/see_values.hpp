#pragma once

#include <array>

#include "core/piece.hpp"
#include "tuning/parameters.hpp"

namespace see {

// Exchange prices (pawn = 100), shared by SEE and capture ordering.
inline constexpr int piece_value(PieceType piece,
                                 const tuning::Parameters& parameters = tuning::defaults) {
    const std::array<int, N_PIECETYPES> values{
        0,
        100,
        parameters.SeeKnight,
        parameters.SeeBishop,
        parameters.SeeRook,
        parameters.SeeQueen,
        0};
    return values[piece];
}

} // namespace see
