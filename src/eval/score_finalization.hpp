#pragma once

#include <algorithm>

#include "core/types.hpp"
#include "eval/parameters.hpp"
#include "eval/tapered_score.hpp"

namespace eval {

struct FinalizedScore {
    TaperedScore scaled;
    Score        tapered;
    Score        side_to_move;
    Score        value;
};

// Production evaluation and feature reconstruction supply their own material
// inputs; the integer conversion from a raw score is shared.
inline FinalizedScore finalize_score(TaperedScore phase_scores,
                                     int          non_pawn_material,
                                     int          white_pawns,
                                     int          black_pawns,
                                     Color        turn) {
    const int pawns = phase_scores.eg < 0 ? black_pawns : white_pawns;
    const int scale = std::min(scale_limit, scale_base + scale_per_pawn * pawns);
    phase_scores.eg = (phase_scores.eg * scale) / scale_limit;

    const int   material = std::clamp(non_pawn_material, material_eg, material_mg);
    const int   phase    = ((material - material_eg) * phase_limit) / (material_mg - material_eg);
    const Score tapered =
        (phase_scores.mg * phase + phase_scores.eg * (phase_limit - phase)) / phase_limit;
    const Score side_to_move = tapered * (turn == WHITE ? 1 : -1);
    return {phase_scores, tapered, side_to_move, side_to_move + tempo_bonus};
}

} // namespace eval
