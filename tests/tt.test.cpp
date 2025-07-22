#include "tt.hpp"

#include "gtest/gtest.h"

class TTTest : public ::testing::Test {
   protected:
    U64 key      = 0x123456789ABCDEF;
    Move move    = Move(Square::A2, Square::A4);
    I16 score    = 100;
    U8 depth     = 5;
    TT_Flag flag = TT_Flag::Exact;

    void SetUp() override { tt.clear(); }
};

TEST_F(TTTest, InitialState) { EXPECT_EQ(nullptr, tt.probe(0x123456789ABCDEF)); }

TEST_F(TTTest, StoreAndProbe) {
    tt.store(key, move, score, depth, flag, 0);
    TT_Entry* entry = tt.probe(key);

    ASSERT_NE(nullptr, entry);
    EXPECT_EQ(move, entry->move);
    EXPECT_EQ(score, entry->score);
    EXPECT_EQ(depth, entry->depth);
    EXPECT_EQ(flag, entry->flag);
}

TEST_F(TTTest, ReplaceEntry) {
    tt.store(key, move, score, depth, flag, 0);

    Move newMove    = Move(Square::E2, Square::E4);
    I16 newScore    = 200;
    U8 newDepth     = 8;
    TT_Flag newFlag = TT_Flag::Lowerbound;

    tt.store(key, newMove, newScore, newDepth, newFlag, 0);

    TT_Entry* entry = tt.probe(key);
    ASSERT_NE(nullptr, entry);
    EXPECT_EQ(newMove, entry->move);
    EXPECT_EQ(newScore, entry->score);
    EXPECT_EQ(newDepth, entry->depth);
    EXPECT_EQ(newFlag, entry->flag);
}

TEST_F(TTTest, ClearTable) {
    tt.store(key, move, score, depth, flag, 0);
    tt.clear();

    EXPECT_EQ(nullptr, tt.probe(key));
}

TEST_F(TTTest, MateScoreAdjustment) {
    I16 mateScore = MATE_VALUE - 5;  // Mate in 3
    int ply       = 2;

    tt.store(key, move, mateScore, depth, flag, ply);
    TT_Entry* entry = tt.probe(key);

    ASSERT_NE(nullptr, entry);
    EXPECT_EQ(mateScore + ply, entry->score);
    EXPECT_EQ(mateScore, entry->get_score(ply));
}

TEST_F(TTTest, NegativeMateScoreAdjustment) {
    I16 mateScore = -MATE_VALUE + 6;  // Mated in 3
    int ply       = 3;

    tt.store(key, move, mateScore, depth, flag, ply);
    TT_Entry* entry = tt.probe(key);

    ASSERT_NE(nullptr, entry);
    EXPECT_EQ(mateScore - ply, entry->score);
    EXPECT_EQ(mateScore, entry->get_score(ply));
}
