#include "search/ordering/history.hpp"

#include <gtest/gtest.h>

#include "search/limits.hpp"

namespace search::ordering {

TEST(QuietHistoryTest, TracksKeysAndLifecycle) {
    QuietHistory hist;
    hist.reward(WHITE, E2, E4, 3);
    hist.penalize(BLACK, E7, E5, 3);
    EXPECT_EQ(hist.get(WHITE, E2, E4), 9);
    EXPECT_EQ(hist.get(BLACK, E7, E5), -9);
    EXPECT_EQ(hist.get(BLACK, E2, E4), 0);
    EXPECT_EQ(hist.get(WHITE, E4, E2), 0);
    hist.age();
    EXPECT_EQ(hist.get(WHITE, E2, E4), 4);
    EXPECT_EQ(hist.get(BLACK, E7, E5), -4);
    hist.clear();
    EXPECT_EQ(hist.get(WHITE, E2, E4), 0);
    EXPECT_EQ(hist.get(BLACK, E7, E5), 0);
    hist.penalize(WHITE, D2, D4, 4, 2);
    EXPECT_EQ(hist.get(WHITE, D2, D4), -8);
    for (int i = 0; i < 2; ++i) {
        hist.reward(WHITE, E2, E4, Limits::max_depth);
        hist.penalize(BLACK, E7, E5, Limits::max_depth);
    }
    EXPECT_EQ(hist.get(WHITE, E2, E4), QuietHistory::max_score);
    EXPECT_EQ(hist.get(BLACK, E7, E5), -QuietHistory::max_score);
}

TEST(CaptureHistoryTest, TracksKeysBoundsAndSignedAging) {
    CaptureHistory hist, other;
    for (int i = 0; i < 3; ++i) {
        hist.reward(WHITE, KNIGHT, E5, PAWN, 4);
        hist.penalize(BLACK, ROOK, A3, QUEEN, 4);
    }
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 757);
    EXPECT_EQ(hist.get(BLACK, ROOK, A3, QUEEN), -757);
    EXPECT_EQ(hist.get(BLACK, KNIGHT, E5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, BISHOP, E5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, D5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, BISHOP), 0);
    EXPECT_EQ(other.get(WHITE, KNIGHT, E5, PAWN), 0);
    hist.age();
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 378);
    EXPECT_EQ(hist.get(BLACK, ROOK, A3, QUEEN), -378);
    hist.clear();
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 0);
    EXPECT_EQ(hist.get(BLACK, ROOK, A3, QUEEN), 0);
    hist.reward(WHITE, KNIGHT, E5, PAWN, Limits::max_depth);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), CaptureHistory::max_score / 8);
    for (int i = 0; i < 256; ++i) {
        hist.reward(WHITE, KNIGHT, E5, PAWN, Limits::max_depth);
        hist.penalize(BLACK, ROOK, A3, QUEEN, Limits::max_depth);
    }
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), CaptureHistory::max_score);
    EXPECT_EQ(hist.get(BLACK, ROOK, A3, QUEEN), -CaptureHistory::max_score);
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

} // namespace search::ordering
