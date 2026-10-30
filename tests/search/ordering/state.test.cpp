#include "search/ordering/state.hpp"

#include <gtest/gtest.h>

namespace search::ordering {

TEST(OrderingStateTest, SearchPreparationResetsRefutationsAndAgesMoveHistories) {
    State      state;
    const Move first_killer{E2, E4};
    const Move second_killer{D2, D4};
    const Move counter{G8, F6};

    state.killers.update(first_killer, 0);
    state.killers.update(second_killer, 0);
    state.counters.update(WHITE, PAWN, E4, counter);
    state.quiets.reward(WHITE, E2, E4, 4);
    state.captures.reward(WHITE, KNIGHT, E5, PAWN, 4);
    state.continuations.reward(WHITE, PAWN, E4, KNIGHT, F6, 4);

    state.prepare_for_search();

    EXPECT_EQ(state.killers.primary(0), NULL_MOVE);
    EXPECT_EQ(state.killers.secondary(0), NULL_MOVE);
    EXPECT_EQ(state.counters.get(WHITE, PAWN, E4), NULL_MOVE);
    EXPECT_EQ(state.quiets.get(WHITE, E2, E4), 8);
    EXPECT_EQ(state.captures.get(WHITE, KNIGHT, E5, PAWN), 128);
    EXPECT_EQ(state.continuations.get(WHITE, PAWN, E4, KNIGHT, F6), 16);
}

TEST(OrderingStateTest, CaptureHistoryIsWorkerLocal) {
    State a, b;
    a.captures.reward(WHITE, KNIGHT, E5, PAWN, 4);
    EXPECT_EQ(a.captures.get(WHITE, KNIGHT, E5, PAWN), 256);
    EXPECT_EQ(b.captures.get(WHITE, KNIGHT, E5, PAWN), 0);
    b.captures.penalize(WHITE, KNIGHT, E5, PAWN, 4, 2);
    EXPECT_EQ(a.captures.get(WHITE, KNIGHT, E5, PAWN), 256);
}
TEST(OrderingStateTest, EnPassantHistoryUsesTheOffTargetPawnVictim) {
    State       state;
    const Board board{"4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1"};
    const Move  move{B4, A3, MOVE_EP};
    state.reward_capture(board, move, 4);
    EXPECT_EQ(state.capture_score(board, move), 256);
    EXPECT_EQ(state.captures.get(BLACK, PAWN, A3, PAWN), 256);
    EXPECT_EQ(state.captures.get(BLACK, PAWN, A3, KNIGHT), 0);
    state.penalize_capture(board, move, 2);
    EXPECT_LT(state.capture_score(board, move), 256);
    state.clear();
    EXPECT_EQ(state.capture_score(board, move), 0);
}

} // namespace search::ordering
