#include "state.hpp"

#include <gtest/gtest.h>

TEST(StateTest, CanCastleTrue) {
    State state;
    EXPECT_TRUE(state.canCastle(WHITE));
    EXPECT_TRUE(state.canCastleOO(WHITE));
    EXPECT_TRUE(state.canCastleOOO(WHITE));
    EXPECT_TRUE(state.canCastle(BLACK));
    EXPECT_TRUE(state.canCastleOO(BLACK));
    EXPECT_TRUE(state.canCastleOOO(BLACK));
}

TEST(StateTest, CanCastleFalse) {
    State state;
    state.castle = NO_CASTLE;
    EXPECT_FALSE(state.canCastle(WHITE));
    EXPECT_FALSE(state.canCastleOO(WHITE));
    EXPECT_FALSE(state.canCastleOOO(WHITE));
    EXPECT_FALSE(state.canCastle(BLACK));
    EXPECT_FALSE(state.canCastleOO(BLACK));
    EXPECT_FALSE(state.canCastleOOO(BLACK));
}

TEST(StateTest, DisableCastle) {
    State state;
    state.disableCastle(WHITE);
    EXPECT_FALSE(state.canCastle(WHITE));
    state.disableCastle(BLACK);
    EXPECT_FALSE(state.canCastle(BLACK));
}

TEST(StateTest, DisableCastleOO) {
    State state;
    state.disableCastle(WHITE, H1);
    EXPECT_TRUE(state.canCastle(WHITE));
    EXPECT_FALSE(state.canCastleOO(WHITE));
    EXPECT_TRUE(state.canCastleOOO(WHITE));
    state.disableCastle(BLACK, H8);
    EXPECT_TRUE(state.canCastle(BLACK));
    EXPECT_FALSE(state.canCastleOO(BLACK));
    EXPECT_TRUE(state.canCastleOOO(BLACK));
}

TEST(StateTest, DisableCastleOOO) {
    State state;
    state.disableCastle(WHITE, A1);
    EXPECT_TRUE(state.canCastle(WHITE));
    EXPECT_TRUE(state.canCastleOO(WHITE));
    EXPECT_FALSE(state.canCastleOOO(WHITE));
    state.disableCastle(BLACK, A8);
    EXPECT_TRUE(state.canCastle(BLACK));
    EXPECT_TRUE(state.canCastleOO(BLACK));
    EXPECT_FALSE(state.canCastleOOO(BLACK));
}

