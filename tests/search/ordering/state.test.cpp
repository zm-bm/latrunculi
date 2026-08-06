#include "search/ordering/state.hpp"

#include <gtest/gtest.h>

namespace search::ordering {

TEST(OrderingStateTest, SearchPreparationResetsRefutationsAndAgesOnlyQuietHistory) {
    State      state;
    const Move first_killer{E2, E4};
    const Move second_killer{D2, D4};
    const Move counter{G8, F6};

    state.killers.update(first_killer, 0);
    state.killers.update(second_killer, 0);
    state.counters.update(WHITE, PAWN, E4, counter);
    state.quiets.reward(WHITE, E2, E4, 4);
    state.continuations.reward(WHITE, PAWN, E4, KNIGHT, F6, 4);

    state.prepare_for_search();

    EXPECT_EQ(state.killers.primary(0), NULL_MOVE);
    EXPECT_EQ(state.killers.secondary(0), NULL_MOVE);
    EXPECT_EQ(state.counters.get(WHITE, PAWN, E4), NULL_MOVE);
    EXPECT_EQ(state.quiets.get(WHITE, E2, E4), 8);
    EXPECT_EQ(state.continuations.get(WHITE, PAWN, E4, KNIGHT, F6), 16);
}

} // namespace search::ordering
