#include "move.hpp"

#include <gtest/gtest.h>

#include "defs.hpp"

TEST(MoveTest, DefaultConstructor) {
    Move move;
    EXPECT_TRUE(move.is_null()) << "should create null move by default";
}

TEST(MoveTest, Constructor) {
    Square from = A2;
    Square to   = A3;

    Move move(from, to);

    EXPECT_EQ(move.from(), from) << "should create move with from square";
    EXPECT_EQ(move.to(), to) << "should create move with to square";
    EXPECT_EQ(move.type(), BASIC_MOVE) << "should create normal move";
    EXPECT_FALSE(move.is_null()) << "should not create null move";
}

TEST(MoveTest, EqualityOperator) {
    Move move1(A2, A3);
    Move move2(A2, A3);
    Move move3(A2, A4);

    EXPECT_EQ(move1, move2) << "should be equal when squares are equal";
    EXPECT_NE(move1, move3) << "should not be equal when squares not equal";
}

TEST(MoveTest, PromotionMove) {
    Move move(A7, A8, MOVE_PROM, QUEEN);
    EXPECT_EQ(move.type(), MOVE_PROM) << "should create promotion move";
    EXPECT_EQ(move.prom_piece(), QUEEN) << "should handle promotion piece";
    EXPECT_FALSE(move.is_null()) << "should not be null move";
}

TEST(MoveTest, OutputStreamOperator) {
    Move move(A7, A8, MOVE_PROM, ROOK);
    EXPECT_EQ(move.str(), "a7a8r") << "should output move string";
}
