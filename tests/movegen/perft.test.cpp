#include "movegen/perft.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "core/constants.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_snapshot.hpp"

namespace {

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results
struct PerftPosition {
    std::string_view         fen;
    std::array<NodeCount, 4> expected;
};

constexpr std::array<PerftPosition, 7> perft_positions = {
    PerftPosition{
        .fen      = board_test::fen::start,
        .expected = {20, 400, 8902, 197281},
    },
    PerftPosition{
        .fen      = board_test::fen::perft_position_2,
        .expected = {48, 2039, 97862, 4085603},
    },
    PerftPosition{
        .fen      = board_test::fen::perft_position_3,
        .expected = {14, 191, 2812, 43238},
    },
    PerftPosition{
        .fen      = board_test::fen::perft_position_4_white,
        .expected = {6, 264, 9467, 422333},
    },
    PerftPosition{
        .fen      = board_test::fen::perft_position_4_black,
        .expected = {6, 264, 9467, 422333},
    },
    PerftPosition{
        .fen      = board_test::fen::perft_position_5,
        .expected = {44, 1486, 62379, 2103487},
    },
    PerftPosition{
        .fen      = board_test::fen::perft_position_6,
        .expected = {46, 2079, 89890, 3894594},
    },
};

} // namespace

TEST(PerftTest, ChessProgrammingPositionsMatchExpectedDepths) {
    for (const auto& position : perft_positions) {
        Board board(position.fen);

        for (int depth = 1; depth <= static_cast<int>(position.expected.size()); ++depth) {
            const NodeCount result = movegen::perft(board, depth);
            EXPECT_EQ(result, position.expected[depth - 1])
                << position.fen << " at depth " << depth;
        }
    }
}

TEST(PerftTest, DepthZeroReturnsOne) {
    Board board(board_test::fen::start);

    EXPECT_EQ(movegen::perft(board, 0), 1U);
    EXPECT_EQ(movegen::format_perft_result(movegen::perft_root(board, 0)), "NODES: 1\n");
}

TEST(PerftTest, RootReturnsMoveBreakdown) {
    Board board(board_test::fen::start);

    const movegen::PerftResult result = movegen::perft_root(board, 1);

    EXPECT_EQ(result.nodes, 20U);
    ASSERT_EQ(result.root_moves.size(), 20U);
    EXPECT_EQ(result.root_moves.front().nodes, 1U);

    const std::string output = movegen::format_perft_result(result);
    EXPECT_NE(output.find(": 1\n"), std::string::npos);
    EXPECT_NE(output.find("NODES: 20\n"), std::string::npos);
}

TEST(PerftTest, RejectsInvalidDepths) {
    Board board(board_test::fen::start);

    EXPECT_THROW(movegen::perft(board, -1), std::invalid_argument);
    EXPECT_THROW(movegen::perft_root(board, -1), std::invalid_argument);
    EXPECT_THROW(movegen::perft(board, engine::max_search_ply + 1), std::invalid_argument);
    EXPECT_THROW(movegen::perft_root(board, engine::max_search_ply + 1), std::invalid_argument);
}

TEST(PerftTest, RestoresBoardState) {
    Board      board(board_test::fen::perft_position_2);
    const auto original = board_test::snapshot_board(board);

    EXPECT_GT(movegen::perft(board, 2), 0U);
    board_test::expect_same_board_snapshot(board, original);

    Board      root_board(board_test::fen::perft_position_2);
    const auto original_root = board_test::snapshot_board(root_board);

    const movegen::PerftResult result = movegen::perft_root(root_board, 2);
    EXPECT_GT(result.nodes, 0U);
    board_test::expect_same_board_snapshot(root_board, original_root);
}
