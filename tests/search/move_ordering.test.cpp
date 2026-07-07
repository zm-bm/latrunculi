#include "search/move_ordering.hpp"

#include <gtest/gtest.h>

#include "search/search_limits.hpp"

TEST(QuietHistoryTest, RewardAndRetrieve) {
    QuietHistory hist;
    hist.reward(WHITE, E2, E4, 3);

    auto value = hist.get(WHITE, E2, E4);
    EXPECT_GT(value, 0);
    EXPECT_EQ(hist.get(BLACK, E2, E4), 0);
    EXPECT_EQ(hist.get(WHITE, E4, E2), 0);

    hist.reward(WHITE, E2, E4, 2);
    EXPECT_GT(hist.get(WHITE, E2, E4), value);
}

TEST(QuietHistoryTest, PenalizeAndRetrieve) {
    QuietHistory hist;
    hist.penalize(WHITE, E2, E4, 3);

    auto value = hist.get(WHITE, E2, E4);
    EXPECT_LT(value, 0);
    EXPECT_EQ(hist.get(BLACK, E2, E4), 0);
    EXPECT_EQ(hist.get(WHITE, E4, E2), 0);

    hist.penalize(WHITE, E2, E4, 2);
    EXPECT_LT(hist.get(WHITE, E2, E4), value);
}

TEST(QuietHistoryTest, PenalizeCanScaleMalus) {
    QuietHistory full;
    QuietHistory half;

    full.penalize(WHITE, E2, E4, 4);
    half.penalize(WHITE, E2, E4, 4, 2);

    EXPECT_EQ(full.get(WHITE, E2, E4), -16);
    EXPECT_EQ(half.get(WHITE, E2, E4), -8);
}

TEST(QuietHistoryTest, UpdatesStayWithinHistoryBand) {
    QuietHistory positive;

    for (int i = 0; i < 8; ++i)
        positive.reward(WHITE, E2, E4, SearchLimits::max_depth);

    EXPECT_GE(positive.get(WHITE, E2, E4), 0);
    EXPECT_LE(positive.get(WHITE, E2, E4), QuietHistory::MAX_SCORE);

    QuietHistory negative;
    for (int i = 0; i < 8; ++i)
        negative.penalize(WHITE, E2, E4, SearchLimits::max_depth);

    EXPECT_LE(negative.get(WHITE, E2, E4), 0);
    EXPECT_GE(negative.get(WHITE, E2, E4), -QuietHistory::MAX_SCORE);
}

TEST(QuietHistoryTest, AgePositiveEntryByDivision) {
    QuietHistory hist;
    hist.reward(WHITE, E2, E4, 3);

    auto value = hist.get(WHITE, E2, E4);
    hist.age();
    EXPECT_GT(hist.get(WHITE, E2, E4), 0);
    EXPECT_LT(hist.get(WHITE, E2, E4), value);
}

TEST(QuietHistoryTest, AgeNegativeEntryByDivision) {
    QuietHistory hist;
    hist.penalize(WHITE, E2, E4, 3);

    ASSERT_EQ(hist.get(WHITE, E2, E4), -9);
    hist.age();
    EXPECT_EQ(hist.get(WHITE, E2, E4), -4);
}

TEST(QuietHistoryTest, Clear) {
    QuietHistory hist;
    hist.reward(WHITE, E2, E4, 3);
    hist.penalize(BLACK, E7, E5, 3);

    hist.clear();
    EXPECT_EQ(hist.get(WHITE, E2, E4), 0);
    EXPECT_EQ(hist.get(BLACK, E7, E5), 0);
}

TEST(CaptureHistoryTest, RewardAndRetrieve) {
    CaptureHistory hist;
    hist.reward(WHITE, KNIGHT, E5, PAWN, 3);

    auto value = hist.get(WHITE, KNIGHT, E5, PAWN);
    EXPECT_GT(value, 0);
    EXPECT_EQ(hist.get(BLACK, KNIGHT, E5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, BISHOP, E5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E4, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, KNIGHT), 0);

    hist.reward(WHITE, KNIGHT, E5, PAWN, 2);
    EXPECT_GT(hist.get(WHITE, KNIGHT, E5, PAWN), value);
}

TEST(CaptureHistoryTest, PenalizeAndRetrieve) {
    CaptureHistory hist;
    hist.penalize(WHITE, KNIGHT, E5, PAWN, 3);

    auto value = hist.get(WHITE, KNIGHT, E5, PAWN);
    EXPECT_LT(value, 0);
    EXPECT_EQ(hist.get(BLACK, KNIGHT, E5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, BISHOP, E5, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E4, PAWN), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, KNIGHT), 0);

    hist.penalize(WHITE, KNIGHT, E5, PAWN, 2);
    EXPECT_LT(hist.get(WHITE, KNIGHT, E5, PAWN), value);
}

TEST(CaptureHistoryTest, PenalizeCanScaleMalus) {
    CaptureHistory full;
    CaptureHistory half;

    full.penalize(WHITE, KNIGHT, E5, PAWN, 4);
    half.penalize(WHITE, KNIGHT, E5, PAWN, 4, 2);

    EXPECT_EQ(full.get(WHITE, KNIGHT, E5, PAWN), -16);
    EXPECT_EQ(half.get(WHITE, KNIGHT, E5, PAWN), -8);
}

TEST(CaptureHistoryTest, UpdatesStayWithinHistoryBand) {
    CaptureHistory positive;
    for (int i = 0; i < 8; ++i)
        positive.reward(WHITE, QUEEN, E5, ROOK, SearchLimits::max_depth);

    EXPECT_GE(positive.get(WHITE, QUEEN, E5, ROOK), 0);
    EXPECT_LE(positive.get(WHITE, QUEEN, E5, ROOK), CaptureHistory::MAX_SCORE);

    CaptureHistory negative;
    for (int i = 0; i < 8; ++i)
        negative.penalize(WHITE, QUEEN, E5, ROOK, SearchLimits::max_depth);

    EXPECT_LE(negative.get(WHITE, QUEEN, E5, ROOK), 0);
    EXPECT_GE(negative.get(WHITE, QUEEN, E5, ROOK), -CaptureHistory::MAX_SCORE);
}

TEST(CaptureHistoryTest, AgePositiveEntryByDivision) {
    CaptureHistory hist;
    hist.reward(WHITE, KNIGHT, E5, PAWN, 3);

    ASSERT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 9);
    hist.age();
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 4);
}

TEST(CaptureHistoryTest, AgeNegativeEntryByDivision) {
    CaptureHistory hist;
    hist.penalize(BLACK, BISHOP, D4, KNIGHT, 3);

    ASSERT_EQ(hist.get(BLACK, BISHOP, D4, KNIGHT), -9);
    hist.age();
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, KNIGHT), -4);
}

TEST(CaptureHistoryTest, Clear) {
    CaptureHistory hist;
    hist.reward(WHITE, KNIGHT, E5, PAWN, 3);
    hist.penalize(BLACK, BISHOP, D4, KNIGHT, 3);

    hist.clear();
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E5, PAWN), 0);
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, KNIGHT), 0);
}

TEST(ContinuationHistoryTest, RewardAndRetrieve) {
    ContinuationHistory hist;
    hist.reward(WHITE, PAWN, E4, KNIGHT, F6, 3);

    auto value = hist.get(WHITE, PAWN, E4, KNIGHT, F6);
    EXPECT_GT(value, 0);
    EXPECT_EQ(hist.get(BLACK, PAWN, E4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, D4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, BISHOP, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, KNIGHT, G4), 0);

    hist.reward(WHITE, PAWN, E4, KNIGHT, F6, 2);
    EXPECT_GT(hist.get(WHITE, PAWN, E4, KNIGHT, F6), value);
}

TEST(ContinuationHistoryTest, PenalizeAndRetrieve) {
    ContinuationHistory hist;
    hist.penalize(WHITE, PAWN, E4, KNIGHT, F6, 3);

    auto value = hist.get(WHITE, PAWN, E4, KNIGHT, F6);
    EXPECT_LT(value, 0);
    EXPECT_EQ(hist.get(BLACK, PAWN, E4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, KNIGHT, E4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, D4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, BISHOP, F6), 0);
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, KNIGHT, G4), 0);

    hist.penalize(WHITE, PAWN, E4, KNIGHT, F6, 2);
    EXPECT_LT(hist.get(WHITE, PAWN, E4, KNIGHT, F6), value);
}

TEST(ContinuationHistoryTest, PenalizeCanScaleMalus) {
    ContinuationHistory full;
    ContinuationHistory half;

    full.penalize(WHITE, PAWN, E4, KNIGHT, F6, 4);
    half.penalize(WHITE, PAWN, E4, KNIGHT, F6, 4, 2);

    EXPECT_EQ(full.get(WHITE, PAWN, E4, KNIGHT, F6), -16);
    EXPECT_EQ(half.get(WHITE, PAWN, E4, KNIGHT, F6), -8);
}

TEST(ContinuationHistoryTest, UpdatesStayWithinHistoryBand) {
    ContinuationHistory positive;
    for (int i = 0; i < 8; ++i)
        positive.reward(WHITE, PAWN, E4, KNIGHT, F6, SearchLimits::max_depth);

    EXPECT_GE(positive.get(WHITE, PAWN, E4, KNIGHT, F6), 0);
    EXPECT_LE(positive.get(WHITE, PAWN, E4, KNIGHT, F6), ContinuationHistory::MAX_SCORE);

    ContinuationHistory negative;
    for (int i = 0; i < 8; ++i)
        negative.penalize(WHITE, PAWN, E4, KNIGHT, F6, SearchLimits::max_depth);

    EXPECT_LE(negative.get(WHITE, PAWN, E4, KNIGHT, F6), 0);
    EXPECT_GE(negative.get(WHITE, PAWN, E4, KNIGHT, F6), -ContinuationHistory::MAX_SCORE);
}

TEST(ContinuationHistoryTest, AgePositiveEntryByDivision) {
    ContinuationHistory hist;
    hist.reward(WHITE, PAWN, E4, KNIGHT, F6, 3);

    ASSERT_EQ(hist.get(WHITE, PAWN, E4, KNIGHT, F6), 9);
    hist.age();
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, KNIGHT, F6), 4);
}

TEST(ContinuationHistoryTest, AgeNegativeEntryByDivision) {
    ContinuationHistory hist;
    hist.penalize(BLACK, BISHOP, D4, PAWN, E3, 3);

    ASSERT_EQ(hist.get(BLACK, BISHOP, D4, PAWN, E3), -9);
    hist.age();
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, PAWN, E3), -4);
}

TEST(ContinuationHistoryTest, Clear) {
    ContinuationHistory hist;
    hist.reward(WHITE, PAWN, E4, KNIGHT, F6, 3);
    hist.penalize(BLACK, BISHOP, D4, PAWN, E3, 3);

    hist.clear();
    EXPECT_EQ(hist.get(WHITE, PAWN, E4, KNIGHT, F6), 0);
    EXPECT_EQ(hist.get(BLACK, BISHOP, D4, PAWN, E3), 0);
}

TEST(KillerMovesTest, AddAndRetrieve) {
    KillerMoves killers;
    killers.update(Move(E2, E4), 0);

    EXPECT_TRUE(killers.is_killer(Move(E2, E4), 0));
    EXPECT_FALSE(killers.is_killer(Move(E2, E3), 0));
    EXPECT_FALSE(killers.is_killer(Move(E2, E4), 1));
}

TEST(KillerMovesTest, LimitSize) {
    KillerMoves killers;
    killers.update(Move(C2, C4), 0);
    killers.update(Move(D2, D4), 0);
    killers.update(Move(E2, E4), 0);

    EXPECT_FALSE(killers.is_killer(Move(C2, C4), 0));
    EXPECT_TRUE(killers.is_killer(Move(D2, D4), 0));
    EXPECT_TRUE(killers.is_killer(Move(E2, E4), 0));
}

TEST(KillerMovesTest, Clear) {
    KillerMoves killers;
    killers.update(Move(E2, E4), 0);
    killers.update(Move(E2, E4), 1);
    killers.clear();

    EXPECT_FALSE(killers.is_killer(Move(E2, E4), 0));
    EXPECT_FALSE(killers.is_killer(Move(E2, E4), 1));
}

TEST(CounterMovesTest, UpdateAndRetrieve) {
    CounterMoves counters;
    Move         counter = Move(G8, F6);

    counters.update(WHITE, PAWN, E4, counter);

    EXPECT_EQ(counters.get(WHITE, PAWN, E4), counter);
}

TEST(CounterMovesTest, SeparatesPreviousMoverColor) {
    CounterMoves counters;
    Move         white_counter = Move(G8, F6);
    Move         black_counter = Move(G1, F3);

    counters.update(WHITE, PAWN, E4, white_counter);
    counters.update(BLACK, PAWN, E4, black_counter);

    EXPECT_EQ(counters.get(WHITE, PAWN, E4), white_counter);
    EXPECT_EQ(counters.get(BLACK, PAWN, E4), black_counter);
}

TEST(CounterMovesTest, SeparatesPreviousMovedPiece) {
    CounterMoves counters;
    Move         pawn_counter   = Move(G8, F6);
    Move         knight_counter = Move(B8, C6);

    counters.update(WHITE, PAWN, E4, pawn_counter);
    counters.update(WHITE, KNIGHT, E4, knight_counter);

    EXPECT_EQ(counters.get(WHITE, PAWN, E4), pawn_counter);
    EXPECT_EQ(counters.get(WHITE, KNIGHT, E4), knight_counter);
}

TEST(CounterMovesTest, SeparatesPreviousDestination) {
    CounterMoves counters;
    Move         e4_counter = Move(G8, F6);
    Move         d4_counter = Move(B8, C6);

    counters.update(WHITE, PAWN, E4, e4_counter);
    counters.update(WHITE, PAWN, D4, d4_counter);

    EXPECT_EQ(counters.get(WHITE, PAWN, E4), e4_counter);
    EXPECT_EQ(counters.get(WHITE, PAWN, D4), d4_counter);
}

TEST(CounterMovesTest, ReplacesExistingCounter) {
    CounterMoves counters;
    Move         first  = Move(G8, F6);
    Move         second = Move(B8, C6);

    counters.update(WHITE, PAWN, E4, first);
    counters.update(WHITE, PAWN, E4, second);

    EXPECT_EQ(counters.get(WHITE, PAWN, E4), second);
}

TEST(CounterMovesTest, Clear) {
    CounterMoves counters;
    Move         counter = Move(G8, F6);

    counters.update(WHITE, PAWN, E4, counter);
    counters.clear();

    EXPECT_EQ(counters.get(WHITE, PAWN, E4), NULL_MOVE);
}
