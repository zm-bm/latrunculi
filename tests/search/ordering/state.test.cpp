#include "search/ordering/state.hpp"

#include <gtest/gtest.h>

namespace search::ordering {

TEST(KillerMovesTest, RotatesTwoDistinctKillersPerPly) {
    KillerMoves killers;
    const Move  first{C2, C4};
    const Move  second{D2, D4};
    const Move  third{E2, E4};
    const Move  other_ply{G2, G3};

    killers.update(first, 0);
    EXPECT_EQ(killers.primary(0), first);
    EXPECT_EQ(killers.secondary(0), NULL_MOVE);

    killers.update(first, 0);
    EXPECT_EQ(killers.primary(0), first);
    EXPECT_EQ(killers.secondary(0), NULL_MOVE);

    killers.update(second, 0);
    EXPECT_EQ(killers.primary(0), second);
    EXPECT_EQ(killers.secondary(0), first);

    killers.update(third, 0);
    EXPECT_EQ(killers.primary(0), third);
    EXPECT_EQ(killers.secondary(0), second);
    EXPECT_FALSE(killers.is_killer(first, 0));

    killers.update(other_ply, 1);
    EXPECT_EQ(killers.primary(1), other_ply);
    EXPECT_EQ(killers.primary(0), third);
}

TEST(CounterMovesTest, IndexesAndReplacesByPreviousMove) {
    CounterMoves counters;
    const Move   white_pawn{G8, F6};
    const Move   black_pawn{G1, F3};
    const Move   white_knight{B8, C6};
    const Move   white_pawn_d4{B1, C3};
    const Move   replacement{F8, B4};

    counters.update(WHITE, PAWN, E4, white_pawn);
    counters.update(BLACK, PAWN, E4, black_pawn);
    counters.update(WHITE, KNIGHT, E4, white_knight);
    counters.update(WHITE, PAWN, D4, white_pawn_d4);

    EXPECT_EQ(counters.get(WHITE, PAWN, E4), white_pawn);
    EXPECT_EQ(counters.get(BLACK, PAWN, E4), black_pawn);
    EXPECT_EQ(counters.get(WHITE, KNIGHT, E4), white_knight);
    EXPECT_EQ(counters.get(WHITE, PAWN, D4), white_pawn_d4);

    counters.update(WHITE, PAWN, E4, replacement);
    EXPECT_EQ(counters.get(WHITE, PAWN, E4), replacement);
}

TEST(MoveOrderingTest, SearchPreparationResetsRefutationsAndAgesOnlyQuietHistory) {
    State      ordering;
    const Move first_killer{E2, E4};
    const Move second_killer{D2, D4};
    const Move counter{G8, F6};

    ordering.killers.update(first_killer, 0);
    ordering.killers.update(second_killer, 0);
    ordering.counters.update(WHITE, PAWN, E4, counter);
    ordering.quiets.reward(WHITE, E2, E4, 4);
    ordering.continuations.reward(WHITE, PAWN, E4, KNIGHT, F6, 4);

    ordering.prepare_for_search();

    EXPECT_EQ(ordering.killers.primary(0), NULL_MOVE);
    EXPECT_EQ(ordering.killers.secondary(0), NULL_MOVE);
    EXPECT_EQ(ordering.counters.get(WHITE, PAWN, E4), NULL_MOVE);
    EXPECT_EQ(ordering.quiets.get(WHITE, E2, E4), 8);
    EXPECT_EQ(ordering.continuations.get(WHITE, PAWN, E4, KNIGHT, F6), 16);
}

} // namespace search::ordering
