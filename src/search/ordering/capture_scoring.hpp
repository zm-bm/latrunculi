#pragma once

#include "board/see_values.hpp"

namespace search::ordering::capture_scoring {

inline constexpr int VictimWeight        = 7;
inline constexpr int MaterialScoreOffset = see::piece_value(QUEEN);
inline constexpr int HistoryScoreDivisor = 16;
inline constexpr int HistoryScoreOffset  = 7168;

static_assert(see::piece_value(PAWN) == 100, "Capture history uses pawn=100 exchange units");

// Fixed-price material score shared by captures and capture evasions.
inline int material_score(int victim_value, int attacker_value) {
    return VictimWeight * victim_value + MaterialScoreOffset - attacker_value;
}

// Scores exclude the stage-band base. The history offset keeps unclassified
// captures in the early band even at the minimum history score.
inline int score(int victim_value, int attacker_value, int scaled_capture_history) {
    return material_score(victim_value, attacker_value) + HistoryScoreOffset
         + VictimWeight * scaled_capture_history;
}

// Recover the scoring-time history snapshot; the table may change between picks.
// The input excludes the stage-band base.
inline int scaled_history_from_score(int order_score, int victim_value, int attacker_value) {
    return (order_score - material_score(victim_value, attacker_value) - HistoryScoreOffset)
         / VictimWeight;
}

inline int main_search_see_threshold(PieceType victim, int scaled_capture_history) {
    return -(see::piece_value(victim) + scaled_capture_history) / 2;
}

} // namespace search::ordering::capture_scoring
