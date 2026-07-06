#include "search/principal_variation.hpp"

#include <gtest/gtest.h>

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

TEST(PrincipalVariationTest, EqualityUsesActiveMoves) {
    EXPECT_EQ(pv_for_move(Move(E2, E4)), pv_for_move(Move(E2, E4)));
    EXPECT_EQ(pv_for_line(Move(E2, E4), Move(E7, E5)), pv_for_line(Move(E2, E4), Move(E7, E5)));
}

TEST(PrincipalVariationTest, EqualityRejectsDifferentLines) {
    EXPECT_NE(pv_for_move(Move(E2, E4)), pv_for_move(Move(D2, D4)));
    EXPECT_NE(pv_for_move(Move(E2, E4)), pv_for_line(Move(E2, E4), Move(E7, E5)));
}

TEST(PrincipalVariationTest, EqualityIgnoresInactiveStorage) {
    PrincipalVariation shortened = pv_for_line(Move(E2, E4), Move(E7, E5));
    shortened                    = pv_for_move(Move(E2, E4));

    EXPECT_EQ(shortened, pv_for_move(Move(E2, E4)));
}
