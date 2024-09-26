#include "magics.hpp"

#include <gtest/gtest.h>

#include "globals.hpp"

class MagicsTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { Magics::init(); }
};

U64 targetBitboard(std::vector<Square> squares);

TEST_F(MagicsTest, BishopMiddleOfBoardNoObstacles) {
  U64 expectedAttacks =
      targetBitboard({B1, C2, D3, F5, G6, H7, A8, H1, G2, F3, D5, C6, B7, A8});

  EXPECT_EQ(Magics::getBishopAttacks(E4, 0), expectedAttacks);
}

TEST_F(MagicsTest, BishopBlockedDiagonals) {
  U64 occupancy = targetBitboard({F5, D5, F3});
  U64 expectedAttacks = targetBitboard({B1, C2, D3, F5, D5, F3});

  EXPECT_EQ(Magics::getBishopAttacks(E4, occupancy), expectedAttacks);
}

TEST_F(MagicsTest, BishopSurroundedByPieces) {
  U64 occupancy = targetBitboard({F5, F3, D5, D3});
  U64 expectedAttacks = targetBitboard({F5, F3, D5, D3});

  EXPECT_EQ(Magics::getBishopAttacks(E4, occupancy), expectedAttacks);
}

TEST_F(MagicsTest, BishopEdgeOfBoardNoObstacles) {
  U64 expectedAttacks = targetBitboard({B2, C3, D4, E5, F6, G7, H8});

  EXPECT_EQ(Magics::getBishopAttacks(A1, 0), expectedAttacks);
}

TEST_F(MagicsTest, BishopEdgeOfBoardWithObstacles) {
  U64 occupancy = targetBitboard({C3});
  U64 expectedAttacks = targetBitboard({B2, C3});

  EXPECT_EQ(Magics::getBishopAttacks(A1, occupancy), expectedAttacks);
}

TEST_F(MagicsTest, RookMiddleOfBoardNoObstacles) {
  U64 expectedAttacks =
      (G::rankmask(RANK4, WHITE) | G::filemask(FILE5, WHITE)) ^ G::BITSET[E4];

  EXPECT_EQ(Magics::getRookAttacks(E4, 0), expectedAttacks);
}

TEST_F(MagicsTest, RookBlocked) {
  U64 occupancy = targetBitboard({D4, E5, G4});
  U64 expectedAttacks = targetBitboard({D4, E5, E3, E2, E1, F4, G4});

  EXPECT_EQ(Magics::getRookAttacks(E4, occupancy), expectedAttacks);
}

TEST_F(MagicsTest, RookSurroundedByPieces) {
  U64 occupancy = targetBitboard({D4, E5, E3, F4});
  U64 expectedAttacks = targetBitboard({D4, E5, E3, F4});

  EXPECT_EQ(Magics::getRookAttacks(E4, occupancy), expectedAttacks);
}

TEST_F(MagicsTest, RookEdgeOfBoardNoObstacles) {
  U64 expectedAttacks =
      (G::rankmask(RANK1, WHITE) | G::filemask(FILE1, WHITE)) ^ G::BITSET[A1];

  EXPECT_EQ(Magics::getRookAttacks(A1, 0), expectedAttacks);
}

TEST_F(MagicsTest, RookEdgeOfBoardWithObstacles) {
  U64 occupancy = targetBitboard({A4, B1});
  U64 expectedAttacks = targetBitboard({A2, A3, A4, B1});

  EXPECT_EQ(Magics::getRookAttacks(A1, occupancy), expectedAttacks);
}
