
#include "board.hpp"

#include <gtest/gtest.h>

#include "globals.hpp"
#include "search.hpp"

class BoardTest : public ::testing::Test {
 protected:
  void SetUp() override { Magics::init(); }
};

TEST_F(BoardTest, BoardToFEN) {
  Board board(G::STARTFEN);
  EXPECT_EQ(board.toFEN(), G::STARTFEN);

  board = Board(G::TESTFENS[0]);
  EXPECT_EQ(board.toFEN(), G::TESTFENS[0]);
}

TEST_F(BoardTest, Accessors) {
  Board board(G::TESTFENS[2]);
  EXPECT_EQ(board.sideToMove(), WHITE);
  EXPECT_FALSE(board.canCastle(WHITE));
  EXPECT_TRUE(board.canCastle(BLACK));
  EXPECT_EQ(board.getKingSq(WHITE), G1);
  EXPECT_EQ(board.getKingSq(BLACK), E8);
  EXPECT_EQ(board.getHmClock(), 0);
  EXPECT_EQ(board.getEnPassant(), INVALID);

  board = Board(G::TESTFENS[3]);
  EXPECT_EQ(board.sideToMove(), BLACK);
  EXPECT_TRUE(board.canCastle(WHITE));
  EXPECT_FALSE(board.canCastle(BLACK));

  board = Board(G::TESTFENS[4]);
  EXPECT_EQ(board.getHmClock(), 1);
}

TEST_F(BoardTest, PieceCounts) {
  Board board(G::STARTFEN);
  EXPECT_EQ(board.count<PAWN>(), 16);
  EXPECT_EQ(board.count<KNIGHT>(), 4);
  EXPECT_EQ(board.count<BISHOP>(), 4);
  EXPECT_EQ(board.count<ROOK>(), 4);
  EXPECT_EQ(board.count<QUEEN>(), 2);
  EXPECT_EQ(board.count<KING>(), 2);
}
