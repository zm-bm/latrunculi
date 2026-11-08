#include <utility>

#include <gtest/gtest.h>

#include "search/search_state.hpp"

namespace search {

namespace {

PV pv_for_move(Move move) {
    PV pv;
    PV child;
    pv.update(move, child);
    return pv;
}

PV pv_for_line(Move first, Move second) {
    PV child = pv_for_move(second);
    PV pv;
    pv.update(first, child);
    return pv;
}

} // namespace

TEST(PVTest, StoresAndCopiesActiveMoves) {
    EXPECT_EQ(pv_for_move(Move(E2, E4)), pv_for_move(Move(E2, E4)));
    EXPECT_EQ(pv_for_line(Move(E2, E4), Move(E7, E5)), pv_for_line(Move(E2, E4), Move(E7, E5)));
    const PV original = pv_for_line(Move(E2, E4), Move(E7, E5));
    PV copy(original);
    EXPECT_EQ(copy, original);
    PV moved(std::move(copy));
    EXPECT_EQ(moved, original);
}

TEST(PVTest, DistinguishesLinesAndTruncatesAtCapacity) {
    EXPECT_NE(pv_for_move(Move(E2, E4)), pv_for_move(Move(D2, D4)));
    EXPECT_NE(pv_for_move(Move(E2, E4)), pv_for_line(Move(E2, E4), Move(E7, E5)));
    PV full;
    for (int i = 0; i < engine::max_search_ply + 2; ++i) {
        const PV child(full);
        full.update(Move(E2, E4), child);
    }
    EXPECT_EQ(full.size(), engine::max_search_ply + 1);
    PV extended;
    extended.update(Move(E7, E5), full);
    EXPECT_EQ(extended.size(), engine::max_search_ply + 1);
    EXPECT_EQ(extended.front(), Move(E7, E5));
    EXPECT_EQ(extended.move_at(engine::max_search_ply), Move(E2, E4));
}

TEST(PVTest, ClearingAndAssignmentIgnoreInactiveStorage) {
    PV shortened = pv_for_line(Move(E2, E4), Move(E7, E5));
    shortened = pv_for_move(Move(E2, E4));

    EXPECT_EQ(shortened, pv_for_move(Move(E2, E4)));
    const PV empty{};
    EXPECT_TRUE(empty.empty());
    shortened.clear();
    EXPECT_EQ(shortened, empty);
    shortened = pv_for_line(Move(D2, D4), Move(D7, D5));
    EXPECT_EQ(shortened.size(), 2);
    EXPECT_EQ(shortened.front(), Move(D2, D4));
    shortened = empty;
    EXPECT_TRUE(shortened.empty());
}

} // namespace search
