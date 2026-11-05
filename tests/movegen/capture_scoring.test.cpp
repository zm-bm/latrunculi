#include "movegen/capture_scoring.hpp"

#include <array>

#include <gtest/gtest.h>

namespace movegen::detail {

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

} // namespace movegen::detail
