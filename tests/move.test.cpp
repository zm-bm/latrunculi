#include <gtest/gtest.h>
#include "move.hpp"
#include "types.hpp"

TEST(MoveTest, DefaultConstructor) {
    Move move;
    EXPECT_TRUE(move.isNullMove());
}

TEST(MoveTest, Constructor) {
    Square from = A2;
    Square to = A3;

    Move move(from, to);

    EXPECT_EQ(move.from(), from);
    EXPECT_EQ(move.to(), to);
    EXPECT_EQ(move.type(), NORMAL);
    EXPECT_FALSE(move.isNullMove());
}

TEST(MoveTest, EqualityOperator) {
    Move move1(A2, A3);
    Move move2(A2, A3);
    Move move3(A2, A4);

    EXPECT_EQ(move1, move2);
    EXPECT_NE(move1, move3);
}

TEST(MoveTest, LessThanOperator) {
    Move move1(A2, A3);
    move1.score = 10;
    Move move2(A2, A3);
    move2.score = 20;

    EXPECT_TRUE(move1 < move2);
    EXPECT_FALSE(move2 < move1);
}

TEST(MoveTest, PromotionMove) {
    Move move(A7, A8, PROMOTION, QUEEN);
    EXPECT_EQ(move.type(), PROMOTION);
    EXPECT_EQ(move.promoPiece(), QUEEN);
    EXPECT_FALSE(move.isNullMove());
}

TEST(MoveTest, OutputStreamOperator) {
    Move move(A7, A8, PROMOTION, ROOK);
    EXPECT_EQ(move.DebugString(), "a7a8r");
}
