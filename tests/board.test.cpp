
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

  auto fen =
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 0";
  board = Board(fen);
  EXPECT_EQ(board.toFEN(), fen);
}

TEST_F(BoardTest, Accessors) {
  Board board(
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
  );
  EXPECT_EQ(board.sideToMove(), WHITE);
  EXPECT_FALSE(board.canCastle(WHITE));
  EXPECT_TRUE(board.canCastle(BLACK));
  EXPECT_EQ(board.getKingSq(WHITE), G1);
  EXPECT_EQ(board.getKingSq(BLACK), E8);
  EXPECT_EQ(board.getHmClock(), 0);
  EXPECT_EQ(board.getEnPassant(), INVALID);

  board = Board(
    "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1"
  );
  EXPECT_EQ(board.sideToMove(), BLACK);
  EXPECT_TRUE(board.canCastle(WHITE));
  EXPECT_FALSE(board.canCastle(BLACK));

  board = Board(
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
  );
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
