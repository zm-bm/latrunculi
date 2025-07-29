#include "killers.hpp"

#include <gtest/gtest.h>

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
