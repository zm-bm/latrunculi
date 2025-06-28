#include "magic.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "constants.hpp"
#include "test_utils.hpp"

struct MagicTest {
    Square square;
    U64 occupancy;
    U64 expectedAttacks;
};

class MagicBishopAttacksTest : public ::testing::TestWithParam<MagicTest> {};

TEST_P(MagicBishopAttacksTest, ComputesAttacks) {
    auto param = GetParam();
    EXPECT_EQ(Magic::getBishopAttacks(param.square, param.occupancy), param.expectedAttacks);
}

INSTANTIATE_TEST_SUITE_P(
    BishopAttacks,
    MagicBishopAttacksTest,
    ::testing::Values(
        // Not blocked
        MagicTest{E4, 0, BB::set(B1, C2, D3, F5, G6, H7, A8, H1, G2, F3, D5, C6, B7, A8)},
        // Partially blocked
        MagicTest{E4, BB::set(F5, D5, F3), BB::set(B1, C2, D3, F5, D5, F3)},
        // Fully blocked
        MagicTest{E4, BB::set(F5, F3, D5, D3), BB::set(F5, F3, D5, D3)},
        // Piece on board edge, not blocked
        MagicTest{A1, 0, BB::set(B2, C3, D4, E5, F6, G7, H8)},
        // Piece on board edge, blocked by one square
        MagicTest{A1, BB::set(C3), BB::set(B2, C3)}));

class MagicRookAttacksTest : public ::testing::TestWithParam<MagicTest> {};

TEST_P(MagicRookAttacksTest, ComputesAttacks) {
    auto param = GetParam();
    EXPECT_EQ(Magic::getRookAttacks(param.square, param.occupancy), param.expectedAttacks);
}

INSTANTIATE_TEST_SUITE_P(
    RookAttacks,
    MagicRookAttacksTest,
    ::testing::Values(
        // Not blocked
        MagicTest{E4, 0, BB::set(E1, E2, E3, E5, E6, E7, E8, A4, B4, C4, D4, F4, G4, H4)},
        // Partially blocked
        MagicTest{E4, BB::set(D4, E5, G4), BB::set(D4, E5, E3, E2, E1, F4, G4)},
        // Fully blocked
        MagicTest{E4, BB::set(D4, E5, E3, F4), BB::set(D4, E5, E3, F4)},
        // Piece on board edge, not blocked
        MagicTest{A1, 0, BB::set(A2, A3, A4, A5, A6, A7, A8, B1, C1, D1, E1, F1, G1, H1)},
        // Piece on board edge, blocked
        MagicTest{A1, BB::set(A4, B1), BB::set(A2, A3, A4, B1)}));
