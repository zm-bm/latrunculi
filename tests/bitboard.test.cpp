#include "bitboard.hpp"

#include <gtest/gtest.h>

#include "types.hpp"

TEST(BitboardTest, clear) {
  U64 bb = 0x1;
  bb &= BB::clear(A1);
  EXPECT_EQ(bb, 0x0);
}

TEST(BitboardTest, set) {
  U64 bb = 0x0;
  bb ^= BB::set(A1);
  EXPECT_EQ(bb, BB::set(A1));
}
