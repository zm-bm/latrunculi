#include <gtest/gtest.h>
#include "globals.hpp"

TEST(BitsetTest, InitializesCorrectly) {
    G::initBitset();

    for (int i = 0; i < 64; ++i) {
        U64 expected_value = 1ULL << i;
        EXPECT_EQ(G::SQUARE_BB[i], expected_value) << "Failed at index " << i;
    }
}

