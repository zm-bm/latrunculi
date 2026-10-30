#pragma once

#include "board/see.hpp"

namespace search::ordering::capture_scoring {

inline constexpr int VictimWeight        = 7;
inline constexpr int QueenScoreBias      = see::piece_value(QUEEN);
inline constexpr int HistoryScoreDivisor = 16;
inline constexpr int HistoryScoreBias    = 7168;
inline constexpr int HistoryUnitsPerPawn = 100;

// Victim and attacker values use fixed SEE prices. The result excludes the
// stage-band base; the history bias keeps unclassified captures in the early band.
inline int score(int victim_value, int attacker_value, int scaled_capture_history) {
    return VictimWeight * victim_value + QueenScoreBias - attacker_value + HistoryScoreBias
         + VictimWeight * scaled_capture_history;
}

// Decode the scoring-time history snapshot, rather than rereading a table that
// earlier searches may have changed. The input excludes the stage-band base.
inline int scaled_history_from_score(int order_score, int victim_value, int attacker_value) {
    return (order_score - VictimWeight * victim_value - QueenScoreBias + attacker_value
            - HistoryScoreBias)
         / VictimWeight;
}

inline int main_search_see_threshold(PieceType victim, int scaled_capture_history) {
    const int correction = scaled_capture_history * see::piece_value(PAWN) / HistoryUnitsPerPawn;
    return -(see::piece_value(victim) + correction) / 2;
}

} // namespace search::ordering::capture_scoring
