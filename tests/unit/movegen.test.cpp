#include "movegen.hpp"

#include <gtest/gtest.h>

#include "board.hpp"
#include "constants.hpp"

TEST(MoveGenTest, All) {
    Board board{STARTFEN};
    MoveGenerator<GenType::All> moves{board};
    EXPECT_EQ(moves.size(), 20) << "should have 20 moves";
}

TEST(MoveGenTest, Captures) {
    Board board{POS2};
    MoveGenerator<GenType::Captures> moves{board};
    EXPECT_EQ(moves.size(), 8) << "should have 8 legal captures";
}

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results

auto startMoves = std::vector<long>{
    20, 400, 8902, 197281, 4865609,
    // 119060324,
};
auto pos2Moves = std::vector<long>{
    48, 2039, 97862, 4085603,
    // 193690690,
    // 8031647685,
};
auto pos3Moves = std::vector<long>{14, 191, 2812, 43238, 674624, 11030083};
auto pos4Moves = std::vector<long>{6, 264, 9467, 422333, 15833292};
auto pos5Moves = std::vector<long>{44, 1486, 62379, 2103487, 89941194};

auto startfen = std::make_tuple(STARTFEN, startMoves);
auto pos2     = std::make_tuple(POS2, pos2Moves);
auto pos3     = std::make_tuple(POS3, pos3Moves);
auto pos4w    = std::make_tuple(POS4W, pos4Moves);
auto pos4b    = std::make_tuple(POS4B, pos4Moves);
auto pos5     = std::make_tuple(POS5, pos5Moves);

using PerftParams = std::tuple<std::string, std::vector<long>>;
class PerftTest : public ::testing::TestWithParam<PerftParams> {};
TEST_P(PerftTest, PerftForMultipleDepths) {
    auto [fen, expected_results] = GetParam();
    Board board(fen);
    std::stringstream nullStream;

    for (int depth = 1; depth <= expected_results.size(); ++depth) {
        long result = board.perft(depth, nullStream);
        EXPECT_EQ(result, expected_results[depth - 1]) << "Failed at depth " << depth;
    }
}

INSTANTIATE_TEST_SUITE_P(StartFenPerftTestSuite, PerftTest, ::testing::Values(startfen));
INSTANTIATE_TEST_SUITE_P(Position2PerftTestSuite, PerftTest, ::testing::Values(pos2));
INSTANTIATE_TEST_SUITE_P(Position3PerftTestSuite, PerftTest, ::testing::Values(pos3));
INSTANTIATE_TEST_SUITE_P(Position4WPerftTestSuite, PerftTest, ::testing::Values(pos4w));
INSTANTIATE_TEST_SUITE_P(Position4BPerftTestSuite, PerftTest, ::testing::Values(pos4b));
INSTANTIATE_TEST_SUITE_P(Position5PerftTestSuite, PerftTest, ::testing::Values(pos5));
