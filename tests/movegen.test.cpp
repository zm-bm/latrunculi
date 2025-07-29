#include "movegen.hpp"

#include <gtest/gtest.h>

#include "board.hpp"
#include "test_util.hpp"

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results

constexpr auto depth_limit = 4;

using PerftParams = std::tuple<std::string, std::vector<long>>;

PerftParams perft_start = {
    STARTFEN,
    {
        20,
        400,
        8902,
        197281,
        4865609,
        119060324,
    },
};

PerftParams perft_pos2 = {
    POS2,
    {
        48,
        2039,
        97862,
        4085603,
        193690690,
        8031647685,
    },
};

PerftParams perft_pos3 = {
    POS3,
    {
        14,
        191,
        2812,
        43238,
        674624,
        11030083,
    },
};

PerftParams perft_pos4w = {
    POS4W,
    {
        6,
        264,
        9467,
        422333,
        15833292,
        706045033,
    },
};

PerftParams perft_pos4b = {
    POS4B,
    {
        6,
        264,
        9467,
        422333,
        15833292,
        706045033,
    },
};

PerftParams perft_pos5 = {
    POS5,
    {
        44,
        1486,
        62379,
        2103487,
        89941194,
    },
};

PerftParams perft_pos6 = {
    POS6,
    {
        46,
        2079,
        89890,
        3894594,
        164075551,
        6923051137,
    },
};

class PerftTest : public ::testing::TestWithParam<PerftParams> {};

TEST_P(PerftTest, PerftDepths) {
    auto [fen, expected] = GetParam();

    Board board(fen);

    std::stringstream null_stream;

    for (int depth = 1; depth <= expected.size(); ++depth) {
        if (depth > depth_limit)
            break;

        long result = board.perft(depth, null_stream);
        EXPECT_EQ(result, expected[depth - 1]) << "Failed at depth " << depth;
    }
}

INSTANTIATE_TEST_SUITE_P(StartFenPerft, PerftTest, ::testing::Values(perft_start));
INSTANTIATE_TEST_SUITE_P(Position2Perft, PerftTest, ::testing::Values(perft_pos2));
INSTANTIATE_TEST_SUITE_P(Position3Perft, PerftTest, ::testing::Values(perft_pos3));
INSTANTIATE_TEST_SUITE_P(Position4WPerft, PerftTest, ::testing::Values(perft_pos4w));
INSTANTIATE_TEST_SUITE_P(Position4BPerft, PerftTest, ::testing::Values(perft_pos4b));
INSTANTIATE_TEST_SUITE_P(Position5Perft, PerftTest, ::testing::Values(perft_pos5));
INSTANTIATE_TEST_SUITE_P(Position6Perft, PerftTest, ::testing::Values(perft_pos6));

TEST(MoveGenTest, All) {
    Board board{STARTFEN};
    auto  movelist = generate<ALL_MOVES>(board);
    EXPECT_EQ(movelist.size(), 20);
}

TEST(MoveGenTest, Captures) {
    Board board{POS2};
    auto  movelist = generate<CAPTURES>(board);
    EXPECT_EQ(movelist.size(), 8);
}
