#include "tt.hpp"

#include "gtest/gtest.h"

class TT_Test : public ::testing::Test {
protected:
    uint64_t key   = 0x123456789ABCDEF;
    Move     move  = Move(Square::A2, Square::A4);
    int16_t  score = 100;
    uint8_t  depth = 5;
    TT_Flag  flag  = TT_Flag::Exact;

    void SetUp() override { tt.clear(); }
};

TEST_F(TT_Test, InitialState) {
    // Check that the table is empty initially
    EXPECT_FALSE(tt.probe(0x123456789ABCDEF).has_value());
}

TEST_F(TT_Test, StoreAndProbe) {
    // Store and retrieve a snapshot
    tt.store(key, move, score, depth, flag, 0);
    auto entry = tt.probe(key);

    // Check that the snapshot was stored correctly
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(move, entry->move);
    EXPECT_EQ(score, entry->score);
    EXPECT_EQ(depth, entry->depth);
    EXPECT_EQ(flag, entry->flag);
}

TEST_F(TT_Test, ReplaceEntry) {
    // Store an initial entry
    tt.store(key, move, score, depth, flag, 0);

    // Replace it with a new entry
    Move    new_move  = Move(Square::E2, Square::E4);
    int16_t new_score = 200;
    uint8_t new_depth = 8;
    TT_Flag new_flag  = TT_Flag::Lowerbound;
    tt.store(key, new_move, new_score, new_depth, new_flag, 0);

    // Check that the snapshot was replaced
    auto entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(new_move, entry->move);
    EXPECT_EQ(new_score, entry->score);
    EXPECT_EQ(new_depth, entry->depth);
    EXPECT_EQ(new_flag, entry->flag);
}

TEST_F(TT_Test, ClearTable) {
    // Store and then clear the table
    tt.store(key, move, score, depth, flag, 0);
    tt.clear();

    // After clearing, the entry should not be found
    EXPECT_FALSE(tt.probe(key).has_value());
}

TEST_F(TT_Test, MateScoreAdjustment) {
    int16_t mate_score = MATE_VALUE - 5; // Mate in 3

    // Store a mate score entry
    int ply = 2;
    tt.store(key, move, mate_score, depth, flag, ply);

    // Entry score is adjusted by ply
    auto entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(mate_score + ply, entry->score);
    EXPECT_EQ(mate_score, entry->get_score(ply));
}

TEST_F(TT_Test, MatedScoreAdjustment) {
    int16_t mate_score = -MATE_VALUE + 6; // Mated in 3

    // Store a mate score entry
    int ply = 5;
    tt.store(key, move, mate_score, depth, flag, ply);

    // Entry score is adjusted by ply
    auto entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(mate_score - ply, entry->score);
    EXPECT_EQ(mate_score, entry->get_score(ply));
}

TEST_F(TT_Test, ResizeTable) {
    // Store an entry
    tt.store(key, move, score, depth, flag, 0);
    EXPECT_TRUE(tt.probe(key).has_value());

    // Resize table
    tt.resize(8);

    // Entry should be gone after resize
    EXPECT_FALSE(tt.probe(key).has_value());

    // We can store and retrieve again
    tt.store(key, move, score, depth, flag, 0);
    auto entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(move, entry->move);
}

TEST_F(TT_Test, EntryKeyGeneration) {
    // Test that different Zobrist keys with the same lowest 16 bits
    // have the same entry key but different cluster keys
    uint64_t key2 = 0xFEDCBA9876543210;
    uint64_t key1 = 0x123456789ABC3210;

    // Store both entries
    tt.store(key1, move, score, depth, flag, 0);
    Move move2 = Move(Square::E2, Square::E4);
    tt.store(key2, move2, 200, 8, TT_Flag::Lowerbound, 0);

    // Both entries should be accessible
    auto entry1 = tt.probe(key1);
    auto entry2 = tt.probe(key2);

    ASSERT_TRUE(entry1.has_value());
    ASSERT_TRUE(entry2.has_value());

    EXPECT_EQ(move, entry1->move);
    EXPECT_EQ(move2, entry2->move);
}

TEST_F(TT_Test, ReplacementScoreCalculation) {
    // Create an entry
    tt.store(key, move, score, depth, flag, 0);
    auto entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());

    // Initial replacement score
    int age           = 0;
    int initial_score = entry->replacement_score(age);

    // The replacement score should now be lower (more likely to be replaced)
    age++;
    int new_score = entry->replacement_score(age);
    EXPECT_LT(new_score, initial_score);

    // A deeper entry should have a higher replacement score
    tt.store(key, move, score, depth + 3, flag, 0);
    entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());

    int deeper_score = entry->replacement_score(age);
    EXPECT_GT(deeper_score, new_score);
}

TEST_F(TT_Test, ProbeReturnsDetachedSnapshot) {
    tt.store(key, move, score, depth, flag, 0);

    auto first_probe = tt.probe(key);
    ASSERT_TRUE(first_probe.has_value());

    Move    new_move  = Move(Square::E2, Square::E4);
    int16_t new_score = 200;
    tt.store(key, new_move, new_score, depth + 1, TT_Flag::Lowerbound, 0);

    EXPECT_EQ(move, first_probe->move);
    EXPECT_EQ(score, first_probe->score);
    EXPECT_EQ(depth, first_probe->depth);
    EXPECT_EQ(flag, first_probe->flag);

    auto second_probe = tt.probe(key);
    ASSERT_TRUE(second_probe.has_value());
    EXPECT_EQ(new_move, second_probe->move);
    EXPECT_EQ(new_score, second_probe->score);
}

TEST_F(TT_Test, StoreAndProbeRoundTripPublishedAge) {
    tt.age_table();
    tt.age_table();

    tt.store(key, move, score, depth, flag, 0);
    auto entry = tt.probe(key);

    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(uint8_t{2}, entry->age);
    EXPECT_EQ(move, entry->move);
    EXPECT_EQ(score, entry->score);
    EXPECT_EQ(depth, entry->depth);
    EXPECT_EQ(flag, entry->flag);
}

TEST_F(TT_Test, NoneFlagEntriesProbeAsMiss) {
    tt.store(key, move, score, depth, TT_Flag::None, 0);
    EXPECT_FALSE(tt.probe(key).has_value());
}
