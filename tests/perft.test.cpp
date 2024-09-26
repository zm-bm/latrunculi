#include <gtest/gtest.h>

#include <memory>

#include "board.hpp"
#include "globals.hpp"
#include "search.hpp"

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results

class PerftTest : public ::testing::TestWithParam<
                      std::tuple<std::string, std::vector<int>>> {
 protected:
  void SetUp() override {
    Magics::init();
    Zobrist::init();
  }
};

TEST_P(PerftTest, PerftForMultipleDepths) {
  auto [fen, expected_results] = GetParam();
  Board board(fen);
  Search search(&board);

  for (int depth = 1; depth <= expected_results.size(); ++depth) {
    int result = search.perft<true, false>(depth);
    EXPECT_EQ(result, expected_results[depth - 1])
        << "Failed at depth " << depth;
  }
}

INSTANTIATE_TEST_SUITE_P(StartFenPerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             G::STARTFEN,
                             std::vector<int>{20, 400, 8902, 197281, 4865609})
                                           // 119060324
                                           ));

auto pos2 = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
INSTANTIATE_TEST_SUITE_P(Position2PerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             pos2, std::vector<int>{48, 2039, 97862, 4085603})
                                           // 193690690
                                           ));

auto pos3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -";
INSTANTIATE_TEST_SUITE_P(Position3PerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             pos3, std::vector<int>{14, 191, 2812, 43238,
                                                    674624, 11030083})));

auto pos4W = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
INSTANTIATE_TEST_SUITE_P(Position4WPerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             pos4W, std::vector<int>{6, 264, 9467, 422333,
                                                     15833292})));

auto pos4B = "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1";
INSTANTIATE_TEST_SUITE_P(Position4BPerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             pos4W, std::vector<int>{6, 264, 9467, 422333,
                                                     15833292})));

auto pos5 = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
INSTANTIATE_TEST_SUITE_P(Position5PerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             pos5, std::vector<int>{44, 1486, 62379, 2103487,
                                                    89941194})));
