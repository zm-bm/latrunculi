#include <gtest/gtest.h>

#include <memory>

#include "board.hpp"
#include "globals.hpp"
#include "search.hpp"

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results

class PerftTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Magics::init();
    Zobrist::init();
  }
};

TEST_F(PerftTest, StartPositionPerft1) {
  auto board = Board(G::STARTFEN);
  Search search(&board);
  int result = search.perft<true, false>(1);
  EXPECT_EQ(result, 20);
}

TEST_F(PerftTest, StartPositionPerft2) {
  auto board = Board(G::STARTFEN);
  Search search(&board);
  int result = search.perft<true, false>(2);
  EXPECT_EQ(result, 400);
}

TEST_F(PerftTest, StartPositionPerft3) {
  auto board = Board(G::STARTFEN);
  Search search(&board);
  int result = search.perft<true, false>(3);
  EXPECT_EQ(result, 8902);
}

TEST_F(PerftTest, StartPositionPerft4) {
  auto board = Board(G::STARTFEN);
  Search search(&board);
  int result = search.perft<true, false>(4);
  EXPECT_EQ(result, 197281);
}

TEST_F(PerftTest, StartPositionPerft5) {
  auto board = Board(G::STARTFEN);
  Search search(&board);
  int result = search.perft<true, false>(5);
  EXPECT_EQ(result, 4865609);
}

// TEST_F(PerftTest, StartPositionPerft6) {
//   auto board = Board(G::STARTFEN);
//   Search search(&board);
//   int result = search.perft<true, false>(6);
//   EXPECT_EQ(result, 119060324);
// }

TEST_F(PerftTest, KiwipetePerft1) {
  auto board =
      Board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  Search search(&board);
  int result = search.perft<true, false>(1);
  EXPECT_EQ(result, 48);
}

TEST_F(PerftTest, KiwipetePerft2) {
  auto board =
      Board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  Search search(&board);
  int result = search.perft<true, false>(2);
  EXPECT_EQ(result, 2039);
}

TEST_F(PerftTest, KiwipetePerft3) {
  auto board =
      Board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  Search search(&board);
  int result = search.perft<true, false>(3);
  EXPECT_EQ(result, 97862);
}

TEST_F(PerftTest, KiwipetePerft4) {
  auto board =
      Board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  Search search(&board);
  int result = search.perft<true, false>(4);
  EXPECT_EQ(result, 4085603);
}

// TEST_F(PerftTest, KiwipetePerft5) {
//   auto board =
//       Board("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
//   Search search(&board);
//   int result = search.perft<true, false>(5);
//   EXPECT_EQ(result, 193690690);
// }

TEST_F(PerftTest, Position3Perft1) {
  auto board = Board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
  Search search(&board);
  int result = search.perft<true, false>(1);
  EXPECT_EQ(result, 14);
}

TEST_F(PerftTest, Position3Perft2) {
  auto board = Board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
  Search search(&board);
  int result = search.perft<true, false>(2);
  EXPECT_EQ(result, 191);
}

TEST_F(PerftTest, Position3Perft3) {
  auto board = Board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
  Search search(&board);
  int result = search.perft<true, false>(3);
  EXPECT_EQ(result, 2812);
}

TEST_F(PerftTest, Position3Perft4) {
  auto board = Board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
  Search search(&board);
  int result = search.perft<true, false>(4);
  EXPECT_EQ(result, 43238);
}

// TEST_F(PerftTest, Position3Perft5) {
//   auto board = Board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
//   Search search(&board);
//   int result = search.perft<true, false>(5);
//   EXPECT_EQ(result, 674624);
// }

// TEST_F(PerftTest, Position3Perft6) {
//   auto board = Board("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
//   Search search(&board);
//   int result = search.perft<true, false>(6);
//   EXPECT_EQ(result, 11030083);
// }

TEST_F(PerftTest, Position4AsWhitePerft1) {
  auto board =
      Board("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -");
  Search search(&board);
  int result = search.perft<true, false>(1);
  EXPECT_EQ(result, 6);
}

TEST_F(PerftTest, Position4AsBlackPerft1) {
  auto board =
      Board("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1");
  Search search(&board);
  int result = search.perft<true, false>(1);
  EXPECT_EQ(result, 6);
}

TEST_F(PerftTest, Position4AsWhitePerft2) {
  auto board =
      Board("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -");
  Search search(&board);
  int result = search.perft<true, false>(2);
  EXPECT_EQ(result, 264);
}

TEST_F(PerftTest, Position4AsBlackPerft2) {
  auto board =
      Board("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1");
  Search search(&board);
  int result = search.perft<true, false>(2);
  EXPECT_EQ(result, 264);
}

TEST_F(PerftTest, Position4AsWhitePerft3) {
  auto board =
      Board("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -");
  Search search(&board);
  int result = search.perft<true, false>(3);
  EXPECT_EQ(result, 9467);
}

TEST_F(PerftTest, Position4AsBlackPerft3) {
  auto board =
      Board("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1");
  Search search(&board);
  int result = search.perft<true, false>(3);
  EXPECT_EQ(result, 9467);
}

TEST_F(PerftTest, Position4AsWhitePerft4) {
  auto board =
      Board("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -");
  Search search(&board);
  int result = search.perft<true, false>(4);
  EXPECT_EQ(result, 422333);
}

TEST_F(PerftTest, Position4AsBlackPerft4) {
  auto board =
      Board("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1");
  Search search(&board);
  int result = search.perft<true, false>(4);
  EXPECT_EQ(result, 422333);
}

TEST_F(PerftTest, Position4AsWhitePerft5) {
  auto board =
      Board("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -");
  Search search(&board);
  int result = search.perft<true, false>(5);
  EXPECT_EQ(result, 15833292);
}

// TEST_F(PerftTest, Position4AsBlackPerft5) {
//   auto board =
//       Board("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1");
//   Search search(&board);
//   int result = search.perft<true, false>(5);
//   EXPECT_EQ(result, 15833292);
// }

TEST_F(PerftTest, Position5Perft1) {
  auto board =
      Board("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
  Search search(&board);
  int result = search.perft<true, false>(1);
  EXPECT_EQ(result, 44);
}

TEST_F(PerftTest, Position5Perft2) {
  auto board =
      Board("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
  Search search(&board);
  int result = search.perft<true, false>(2);
  EXPECT_EQ(result, 1486);
}

TEST_F(PerftTest, Position5Perft3) {
  auto board =
      Board("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
  Search search(&board);
  int result = search.perft<true, false>(3);
  EXPECT_EQ(result, 62379);
}

TEST_F(PerftTest, Position5Perft4) {
  auto board =
      Board("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
  Search search(&board);
  int result = search.perft<true, false>(4);
  EXPECT_EQ(result, 2103487);
}

// TEST_F(PerftTest, Position5Perft5) {
//   auto board =
//       Board("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
//   Search search(&board);
//   int result = search.perft<true, false>(5);
//   EXPECT_EQ(result, 89941194);
// }
