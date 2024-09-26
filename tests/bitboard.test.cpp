#include "bitboard.hpp"

#include <gtest/gtest.h>

#include "types.hpp"

TEST(BitboardTest, clear) {
  auto bb = BBz(0x1);
  bb.clear(A1);
  EXPECT_EQ(bb, BBz(0x0));
}

TEST(BitboardTest, toggleSquare) {
  auto bb = BBz(0x0);
  bb.toggle(A1);
  EXPECT_EQ(bb, BBz(0x1));
}

TEST(BitboardTest, toggleBitboard) {
  auto bb = BBz(0x0);
  bb.toggle(0x1);
  EXPECT_EQ(bb, BBz(0x1));
}
