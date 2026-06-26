#include "move_list.hpp"

#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace {
void build_single_move_list_with_stale_tail(MoveList& movelist) {
    movelist.add(A2, A3);
    movelist.add(B2, B3);
    movelist.clear();
    movelist.add(C2, C3);
}
} // namespace

TEST(MoveListTest, MoveIsValueOnly) {
    Move lhs(E2, E4);
    Move rhs(E2, E4);

    EXPECT_EQ(sizeof(Move), sizeof(uint16_t));
    EXPECT_TRUE(std::is_trivially_copyable_v<Move>);
    EXPECT_EQ(lhs, rhs);
    EXPECT_EQ(lhs.bits, rhs.bits);
}

TEST(MoveListTest, AddMoveAndClearMaintainAppendRange) {
    MoveList movelist;
    Move     first(E2, E4);
    Move     second(G1, F3);

    EXPECT_TRUE(movelist.empty());
    EXPECT_EQ(movelist.begin(), movelist.end());

    movelist.add(first);
    movelist.add(second);

    ASSERT_EQ(movelist.size(), 2U);
    EXPECT_FALSE(movelist.empty());
    EXPECT_EQ(movelist[0], first);
    EXPECT_EQ(movelist[1], second);
    EXPECT_EQ(movelist.end(), movelist.begin() + 2);

    movelist.clear();

    EXPECT_TRUE(movelist.empty());
    EXPECT_EQ(movelist.size(), 0U);
    EXPECT_EQ(movelist.begin(), movelist.end());

    movelist.add(A7, A8, MOVE_PROM, QUEEN);

    ASSERT_EQ(movelist.size(), 1U);
    EXPECT_EQ(movelist[0], Move(A7, A8, MOVE_PROM, QUEEN));
}

TEST(MoveListTest, CopyConstructorCopiesActiveRangeOnly) {
    MoveList source;
    build_single_move_list_with_stale_tail(source);

    MoveList copy(source);

    ASSERT_EQ(copy.size(), 1U);
    EXPECT_EQ(copy[0], Move(C2, C3));
    EXPECT_EQ(copy[1], NULL_MOVE);
}

TEST(MoveListTest, CopyAssignmentCopiesActiveRangeOnly) {
    MoveList source;
    build_single_move_list_with_stale_tail(source);

    MoveList target;
    target.add(H2, H3);
    target.add(G2, G3);

    target = source;

    ASSERT_EQ(target.size(), 1U);
    EXPECT_EQ(target[0], Move(C2, C3));
    EXPECT_EQ(target[1], Move(G2, G3));
}

TEST(MoveListTest, MoveConstructorCopiesActiveRangeOnly) {
    MoveList source;
    build_single_move_list_with_stale_tail(source);

    MoveList moved(std::move(source));

    ASSERT_EQ(moved.size(), 1U);
    EXPECT_EQ(moved[0], Move(C2, C3));
    EXPECT_EQ(moved[1], NULL_MOVE);
}

TEST(MoveListTest, MoveAssignmentCopiesActiveRangeOnly) {
    MoveList source;
    build_single_move_list_with_stale_tail(source);

    MoveList target;
    target.add(H2, H3);
    target.add(G2, G3);

    target = std::move(source);

    ASSERT_EQ(target.size(), 1U);
    EXPECT_EQ(target[0], Move(C2, C3));
    EXPECT_EQ(target[1], Move(G2, G3));
}
