#include "search/ordering/capture_scoring.hpp"

#include <array>

#include <gtest/gtest.h>

namespace search::ordering {

TEST(CaptureScoringTest, RanksCapturesWithSignedHistory) {
    struct Case {
        PieceType victim;
        PieceType attacker;
        int       history;
        int       expected;
    };
    constexpr std::array cases{
        Case{PAWN, KING, 0, 1920},
        Case{PAWN, QUEEN, 0, 700},
        Case{BISHOP, KNIGHT, 0, 3565},
        Case{QUEEN, PAWN, 0, 9660},
        Case{PAWN, QUEEN, -1024, -6468},
        Case{PAWN, QUEEN, -1, 693},
        Case{PAWN, QUEEN, 1, 707},
        Case{PAWN, QUEEN, 1024, 7868},
    };
    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.victim);
        SCOPED_TRACE(tc.attacker);
        SCOPED_TRACE(tc.history);
        EXPECT_EQ(capture_scoring::score(
                      see::piece_value(tc.victim), see::piece_value(tc.attacker), tc.history),
                  tc.expected);
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
