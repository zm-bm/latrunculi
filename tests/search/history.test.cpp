#include "search/history.hpp"

#include <gtest/gtest.h>

#include "search/search_limits.hpp"

TEST(QuietHistoryTest, UpdatesAndIsolatesMoveKeys) {
    QuietHistory hist;

    hist.reward(WHITE, E2, E4, 3);
    hist.penalize(BLACK, E7, E5, 3);

    EXPECT_EQ(hist.get(WHITE, E2, E4), 9);
    EXPECT_EQ(hist.get(BLACK, E7, E5), -9);
    EXPECT_EQ(hist.get(BLACK, E2, E4), 0);
    EXPECT_EQ(hist.get(WHITE, E7, E5), 0);
    EXPECT_EQ(hist.get(WHITE, E4, E2), 0);

    hist.reward(WHITE, E2, E4, 2);
    EXPECT_GT(hist.get(WHITE, E2, E4), 9);
}

TEST(QuietHistoryTest, ScalesMalusesAndBoundsRepeatedUpdates) {
    QuietHistory hist;

    hist.penalize(WHITE, E2, E4, 4);
    hist.penalize(WHITE, D2, D4, 4, 2);
    EXPECT_EQ(hist.get(WHITE, E2, E4), -16);
    EXPECT_EQ(hist.get(WHITE, D2, D4), -8);

    for (int i = 0; i < 8; ++i) {
        hist.reward(WHITE, A2, A3, SearchLimits::max_depth);
        hist.penalize(BLACK, A7, A6, SearchLimits::max_depth);
    }

    EXPECT_EQ(hist.get(WHITE, A2, A3), QuietHistory::max_score);
    EXPECT_EQ(hist.get(BLACK, A7, A6), -QuietHistory::max_score);
}

TEST(QuietHistoryTest, AgesAndClearsSignedEntries) {
    QuietHistory hist;

    hist.reward(WHITE, E2, E4, 3);
    hist.penalize(BLACK, E7, E5, 3);
    hist.age();

    EXPECT_EQ(hist.get(WHITE, E2, E4), 4);
    EXPECT_EQ(hist.get(BLACK, E7, E5), -4);

    hist.clear();
    EXPECT_EQ(hist.get(WHITE, E2, E4), 0);
    EXPECT_EQ(hist.get(BLACK, E7, E5), 0);
}

TEST(CaptureHistoryTest, TracksCaptureKeysAndLifecycle) {
    CaptureHistory hist;

    hist.reward(WHITE, KNIGHT, E5, PAWN, 3);
    hist.penalize(BLACK, BISHOP, D4, KNIGHT, 4, 2);

    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 9);
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, KNIGHT), -8);
    EXPECT_EQ(hist.get(BLACK, KNIGHT, E5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, BISHOP, E5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E4, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, KNIGHT), 0);

    hist.age();
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 4);
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, KNIGHT), -4);

    hist.clear();
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 0);
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, KNIGHT), 0);
}

TEST(ContinuationHistoryTest, TracksContinuationKeysAndLifecycle) {
    ContinuationHistory hist;

    hist.reward(WHITE, PAWN, E4, KNIGHT, F6, 3);
    hist.penalize(BLACK, BISHOP, D4, PAWN, E3, 4, 2);

    EXPECT_EQ(hist.get(WHITE, PAWN, E4, KNIGHT, F6), 9);
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, PAWN, E3), -8);
    EXPECT_EQ(hist.get(BLACK, PAWN, E4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, D4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, BISHOP, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, KNIGHT, G4), 0);

    hist.clear();
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, PAWN, E3), 0);
}
