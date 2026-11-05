#pragma once

#include "board/see_values.hpp"

namespace movegen::detail::capture_scoring {

inline constexpr int VictimWeight        = 7;
inline constexpr int MaterialScoreOffset = see::piece_value(QUEEN);
inline constexpr int HistoryScoreDivisor = 16;

static_assert(see::piece_value(PAWN) == 100, "Capture history uses pawn=100 exchange units");

// Fixed-price material score shared by captures and capture evasions.
inline int material_score(int victim_value, int attacker_value) {
    return VictimWeight * victim_value + MaterialScoreOffset - attacker_value;
}

// Noisy capture scores only rank moves; stage membership is stored separately.
inline int score(int victim_value, int attacker_value, int scaled_capture_history) {
    return material_score(victim_value, attacker_value) + VictimWeight * scaled_capture_history;
}

inline int main_search_see_threshold(PieceType victim, int scaled_capture_history) {
    return -(see::piece_value(victim) + scaled_capture_history) / 2;
}

} // namespace movegen::detail::capture_scoring
