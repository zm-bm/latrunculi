#include "movegen/move_list.hpp"

#include <utility>

#include <gtest/gtest.h>

TEST(MoveListTest, AppendsMovesAndExposesActiveRange) {
    movegen::MoveList movelist;
    Move              first(E2, E4);
    Move              second(G1, F3);

    EXPECT_TRUE(movelist.empty());
    EXPECT_EQ(movelist.begin(), movelist.end());

    movelist.add(first);
    movelist.add(second);

    ASSERT_EQ(movelist.size(), 2U);
    EXPECT_FALSE(movelist.empty());
    EXPECT_EQ(movelist[0], first);
    EXPECT_EQ(movelist[1], second);
    EXPECT_EQ(movelist.end(), movelist.begin() + 2);
}

TEST(MoveListTest, CopyConstructionPreservesActiveRange) {
    movegen::MoveList source;
    source.add(A2, A3);
    source.add(B2, B3);

    movegen::MoveList copy(source);

    ASSERT_EQ(copy.size(), 2U);
    EXPECT_EQ(copy[0], Move(A2, A3));
    EXPECT_EQ(copy[1], Move(B2, B3));
}

TEST(MoveListTest, CopyAssignmentPreservesActiveRange) {
    movegen::MoveList source;
    source.add(A2, A3);
    source.add(B2, B3);

    movegen::MoveList target;
    target.add(H2, H3);

    target = source;

    ASSERT_EQ(target.size(), 2U);
    EXPECT_EQ(target[0], Move(A2, A3));
    EXPECT_EQ(target[1], Move(B2, B3));
}

TEST(MoveListTest, MoveConstructionPreservesActiveRange) {
    movegen::MoveList source;
    source.add(A2, A3);
    source.add(B2, B3);

    movegen::MoveList moved(std::move(source));

    ASSERT_EQ(moved.size(), 2U);
    EXPECT_EQ(moved[0], Move(A2, A3));
    EXPECT_EQ(moved[1], Move(B2, B3));
}

TEST(MoveListTest, MoveAssignmentPreservesActiveRange) {
    movegen::MoveList source;
    source.add(A2, A3);
    source.add(B2, B3);

    movegen::MoveList target;
    target.add(H2, H3);

    target = std::move(source);

    ASSERT_EQ(target.size(), 2U);
    EXPECT_EQ(target[0], Move(A2, A3));
    EXPECT_EQ(target[1], Move(B2, B3));
}
