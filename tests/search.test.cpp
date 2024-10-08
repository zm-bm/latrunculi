#include <gtest/gtest.h>

#include <memory>

#include "chess.hpp"
#include "globals.hpp"
#include "search.hpp"

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results

class PerftTest : public ::testing::TestWithParam<
                      std::tuple<std::string, std::vector<long>>> {
   protected:
    void SetUp() override {
        Magics::init();
        Zobrist::init();
    }
};

TEST_P(PerftTest, PerftForMultipleDepths) {
    auto [fen, expected_results] = GetParam();
    Chess chess(fen);
    Search search(&chess);

    for (int depth = 1; depth <= expected_results.size(); ++depth) {
        long result = search.perft<true, false>(depth);
        EXPECT_EQ(result, expected_results[depth - 1])
            << "Failed at depth " << depth;
    }
}

INSTANTIATE_TEST_SUITE_P(StartFenPerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             G::STARTFEN, std::vector<long>{
                                              20, 400, 8902, 197281, 4865609,
                                            //   119060324,
                                          })));

INSTANTIATE_TEST_SUITE_P(
    Position2PerftTestSuite, PerftTest,
    ::testing::Values(std::make_tuple(G::POS2, std::vector<long>{
                                                48, 2039, 97862, 4085603,
                                                // 193690690,
                                                // 8031647685,
                                            })));

INSTANTIATE_TEST_SUITE_P(Position3PerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             G::POS3, std::vector<long>{14, 191, 2812, 43238,
                                                    674624, 11030083})));

INSTANTIATE_TEST_SUITE_P(Position4WPerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             G::POS4W, std::vector<long>{6, 264, 9467, 422333,
                                                     15833292})));

INSTANTIATE_TEST_SUITE_P(Position4BPerftTestSuite, PerftTest,
                         ::testing::Values(std::make_tuple(
                             G::POS4B, std::vector<long>{6, 264, 9467, 422333,
                                                     15833292})));

INSTANTIATE_TEST_SUITE_P(
    Position5PerftTestSuite, PerftTest,
    ::testing::Values(std::make_tuple(G::POS5, std::vector<long>{
                                                44, 1486, 62379, 2103487,
                                                // 89941194
                                            })));
