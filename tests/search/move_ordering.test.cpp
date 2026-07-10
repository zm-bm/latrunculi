#include "search/move_ordering.hpp"

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
