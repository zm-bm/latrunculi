#include "search/ordering/capture_scoring.hpp"

#include <array>

#include <gtest/gtest.h>

namespace search::ordering {

TEST(CaptureScoringTest, KeepsScoresWithinStageAndRecoversHistory) {
    for (PieceType victim : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN})
        for (PieceType attacker : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING})
            for (int scaled_capture_history : {-1024, -1, 0, 1, 1024}) {
                const int victim_value   = see::piece_value(victim);
                const int attacker_value = see::piece_value(attacker);
                const int order_score =
                    capture_scoring::score(victim_value, attacker_value, scaled_capture_history);
                EXPECT_GT(order_score, 0);
                EXPECT_LT(order_score, 1 << 22);
                EXPECT_EQ(capture_scoring::scaled_history_from_score(
                              order_score, victim_value, attacker_value),
                          scaled_capture_history);
            }
}

TEST(CaptureScoringTest, MainSearchSeeThresholdPreservesSignedTruncation) {
    struct Case {
        PieceType victim;
        int       scaled_capture_history;
        int       expected_threshold;
    };
    constexpr std::array cases{
        Case{PAWN, 0, -50},
        Case{PAWN, -1, -49},
        Case{PAWN, 1, -50},
        Case{PAWN, -2, -49},
        Case{PAWN, 2, -51},
        Case{PAWN, -101, 0},
        Case{PAWN, -102, 1},
        Case{BISHOP, 0, -192},
    };
    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.victim);
        SCOPED_TRACE(tc.scaled_capture_history);
        EXPECT_EQ(capture_scoring::main_search_see_threshold(tc.victim, tc.scaled_capture_history),
                  tc.expected_threshold);
    }
}

} // namespace search::ordering
