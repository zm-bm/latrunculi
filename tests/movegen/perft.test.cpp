#include "movegen/perft.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/constants.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"
#include "support/board_snapshot.hpp"

namespace {

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results
constexpr int depth_limit = 4;

struct PerftPosition {
    std::string            fen;
    std::vector<NodeCount> expected;
};

const std::vector<PerftPosition> perft_positions = {
    {
        .fen      = board_test::fen::start,
        .expected = {20, 400, 8902, 197281, 4865609, 119060324},
    },
    {
        .fen      = board_test::fen::perft_position_2,
        .expected = {48, 2039, 97862, 4085603, 193690690, 8031647685},
    },
    {
        .fen      = board_test::fen::perft_position_3,
        .expected = {14, 191, 2812, 43238, 674624, 11030083},
    },
    {
        .fen      = board_test::fen::perft_position_4_white,
        .expected = {6, 264, 9467, 422333, 15833292, 706045033},
    },
    {
        .fen      = board_test::fen::perft_position_4_black,
        .expected = {6, 264, 9467, 422333, 15833292, 706045033},
    },
    {
        .fen      = board_test::fen::perft_position_5,
        .expected = {44, 1486, 62379, 2103487, 89941194},
    },
    {
        .fen      = board_test::fen::perft_position_6,
        .expected = {46, 2079, 89890, 3894594, 164075551, 6923051137},
    },
};

} // namespace

TEST(PerftTest, ChessProgrammingPositionsMatchExpectedDepths) {
    for (const auto& position : perft_positions) {
        board_test::Harness board(position.fen);

        for (int depth = 1; depth <= static_cast<int>(position.expected.size()); ++depth) {
            if (depth > depth_limit)
                break;

            const NodeCount result = perft(board, depth);
            EXPECT_EQ(result, position.expected[depth - 1]) << "Failed at depth " << depth;
        }
    }
}

TEST(PerftTest, DepthZeroReturnsOne) {
    board_test::Harness board(board_test::fen::start);

    EXPECT_EQ(perft(board, 0), 1U);
    EXPECT_EQ(format_perft_result(perft_root(board, 0)), "NODES: 1\n");
}

TEST(PerftTest, RootReturnsMoveBreakdown) {
    board_test::Harness board(board_test::fen::start);

    const PerftResult result = perft_root(board, 1);

    EXPECT_EQ(result.nodes, 20U);
    ASSERT_EQ(result.root_moves.size(), 20U);
    EXPECT_EQ(result.root_moves.front().nodes, 1U);

    const std::string output = format_perft_result(result);
    EXPECT_NE(output.find(": 1\n"), std::string::npos);
    EXPECT_NE(output.find("NODES: 20\n"), std::string::npos);
}

TEST(PerftTest, RejectsInvalidDepths) {
    board_test::Harness board(board_test::fen::start);

    EXPECT_THROW(perft(board, -1), std::invalid_argument);
    EXPECT_THROW(perft_root(board, -1), std::invalid_argument);
    EXPECT_THROW(perft(board, engine::max_search_ply + 1), std::invalid_argument);
    EXPECT_THROW(perft_root(board, engine::max_search_ply + 1), std::invalid_argument);
}

TEST(PerftTest, RestoresBoardState) {
    board_test::Harness board(board_test::fen::perft_position_2);
    const auto          original = board_test::snapshot_board(board);

    EXPECT_GT(perft(board, 2), 0U);
    board_test::expect_same_board_snapshot(board, original);

    board_test::Harness root_board(board_test::fen::perft_position_2);
    const auto          original_root = board_test::snapshot_board(root_board);

    const PerftResult result = perft_root(root_board, 2);
    EXPECT_GT(result.nodes, 0U);
    board_test::expect_same_board_snapshot(root_board, original_root);
}
