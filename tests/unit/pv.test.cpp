#include "pv.hpp"

#include "gtest/gtest.h"
#include "move.hpp"

TEST(PrincipalVariationTest, UpdateAndBestLine) {
    PrincipalVariation pv;

    // Initially, bestLine should be empty
    EXPECT_TRUE(pv.bestLine().empty());

    // Update ply 0 with a move
    Move move1(A2, A3);
    pv.update(0, move1);

    // After update, bestLine should contain only move1
    MoveLine best = pv.bestLine();
    ASSERT_EQ(best.size(), 1);
    EXPECT_EQ(best[0], move1);

    // bestMove should return the first move
    EXPECT_EQ(pv.bestMove(), move1);

    // Update ply 1 with move2
    Move move2(B2, B3);
    pv.update(1, move2);

    // Updating ply 0 again will prepend move1 and then include line from ply1.
    pv.update(0, move1);
    best = pv.bestLine();
    ASSERT_EQ(best.size(), 2);
    EXPECT_EQ(best[0], move1);
    EXPECT_EQ(best[1], move2);
}

TEST(PrincipalVariationTest, ClearMethods) {
    PrincipalVariation pv;
    Move move1(A2, A3);
    Move move2(B2, B3);
    Move move3(C2, C3);

    // Fill in some moves in different plies
    pv.update(0, move1);
    pv.update(1, move2);
    pv.update(2, move3);

    // Clearing ply 1 should only remove moves in that ply
    pv.clear(1);

    // ply 0 was not cleared, so bestLine remains
    MoveLine best = pv.bestLine();
    ASSERT_EQ(best.size(), 1);
    EXPECT_EQ(best[0], move1);

    // Clear all plies
    pv.clear();
    best = pv.bestLine();
    EXPECT_TRUE(best.empty());
}

TEST(PrincipalVariationTest, OperatorIndexAndStringConversion) {
    PrincipalVariation pv;
    Move move1(A2, A3);
    Move move2(B2, B3);

    // Update ply 0 and ply 1
    pv.update(0, move1);
    pv.update(1, move2);

    // Updating ply 0 again concatenates the line from ply 1
    pv.update(0, move1);

    // Test operator[]: accessing ply 0 should return the same line as bestLine
    MoveLine line0 = pv[0];
    MoveLine best  = pv.bestLine();
    EXPECT_EQ(line0, best);

    // Expect the string to contain move1's representation.
    std::string lineStr = static_cast<std::string>(pv);
    EXPECT_NE(lineStr.find(move1.str()), std::string::npos);
}
