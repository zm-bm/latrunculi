#include <utility>

#include <gtest/gtest.h>

#include "search/principal_variation.hpp"

namespace search {

namespace {

PrincipalVariation pv_for_move(Move move) {
    PrincipalVariation pv;
    PrincipalVariation child;
    pv.update(move, child);
    return pv;
}

PrincipalVariation pv_for_line(Move first, Move second) {
    PrincipalVariation child = pv_for_move(second);
    PrincipalVariation pv;
    pv.update(first, child);
    return pv;
}

} // namespace

TEST(PrincipalVariationTest, StoresAndCopiesActiveMoves) {
    EXPECT_EQ(pv_for_move(Move(E2, E4)), pv_for_move(Move(E2, E4)));
    EXPECT_EQ(pv_for_line(Move(E2, E4), Move(E7, E5)), pv_for_line(Move(E2, E4), Move(E7, E5)));
    const PrincipalVariation original = pv_for_line(Move(E2, E4), Move(E7, E5));
    PrincipalVariation       copy(original);
    EXPECT_EQ(copy, original);
    PrincipalVariation moved(std::move(copy));
    EXPECT_EQ(moved, original);
}

TEST(PrincipalVariationTest, DistinguishesLinesAndTruncatesAtCapacity) {
    EXPECT_NE(pv_for_move(Move(E2, E4)), pv_for_move(Move(D2, D4)));
    EXPECT_NE(pv_for_move(Move(E2, E4)), pv_for_line(Move(E2, E4), Move(E7, E5)));
    PrincipalVariation full;
    for (int i = 0; i < engine::max_search_ply + 2; ++i) {
        const PrincipalVariation child(full);
        full.update(Move(E2, E4), child);
    }
    EXPECT_EQ(full.size(), engine::max_search_ply + 1);
    PrincipalVariation extended;
    extended.update(Move(E7, E5), full);
    EXPECT_EQ(extended.size(), engine::max_search_ply + 1);
    EXPECT_EQ(extended.front(), Move(E7, E5));
    EXPECT_EQ(extended.move_at(engine::max_search_ply), Move(E2, E4));
}

TEST(PrincipalVariationTest, ClearingAndAssignmentIgnoreInactiveStorage) {
    PrincipalVariation shortened = pv_for_line(Move(E2, E4), Move(E7, E5));
    shortened                    = pv_for_move(Move(E2, E4));

    EXPECT_EQ(shortened, pv_for_move(Move(E2, E4)));
    const PrincipalVariation empty{};
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
