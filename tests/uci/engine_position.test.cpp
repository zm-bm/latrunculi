#include "support/engine_test_fixture.hpp"

#include <format>
#include <string>
#include <string_view>

#include "support/board_fixtures.hpp"
#include "support/board_snapshot.hpp"
#include "gtest/gtest.h"

class EnginePositionTest : public EngineTest {
protected:
    static void expect_same_board_and_history(Board actual, Board expected) {
        while (true) {
            board_test::expect_same_board_snapshot(actual, board_test::snapshot_board(expected));
            EXPECT_EQ(actual.is_draw(), expected.is_draw());

            if (!expected.can_unmake())
                break;

            ASSERT_TRUE(actual.can_unmake());
            actual.unmake();
            expected.unmake();
        }
    }
};

TEST_F(EnginePositionTest, PositionStartposResetsFromNonStartPosition) {
    EXPECT_TRUE(execute(std::format("position fen {}", board_test::fen::kings_only)));
    ASSERT_EQ(board().to_fen(), board_test::fen::kings_only);

    EXPECT_TRUE(execute("position startpos"));

    EXPECT_EQ(board().to_fen(), board_test::fen::start);
}

TEST_F(EnginePositionTest, PositionMovesBuildUndoableHistory) {
    EXPECT_TRUE(execute("position startpos moves e2e4 e7e5"));
    ASSERT_TRUE(board().can_unmake());

    board().unmake();
    EXPECT_EQ(board().to_fen(), board_test::fen::after_e2e4);

    board().unmake();
    EXPECT_FALSE(board().can_unmake());
    EXPECT_EQ(board().to_fen(), board_test::fen::start);
}

TEST_F(EnginePositionTest, PositionMovesBuildRepetitionHistoryThatFenReplacementClears) {
    const std::string command = std::format("position fen {} moves "
                                            "e6f5 h7g8 f5e6 g8h7 e6f5 h7g8 f5e6 g8h7 e6f5",
                                            board_test::fen::repetition_cycle);
    ASSERT_TRUE(execute(command));
    ASSERT_TRUE(board().is_draw());

    const std::string repeated_position = board().to_fen();
    ASSERT_TRUE(execute(std::format("position fen {}", repeated_position)));

    EXPECT_FALSE(board().can_unmake());
    EXPECT_FALSE(board().is_draw());
}

TEST_F(EnginePositionTest, PositionReportsInvalidMoveToken) {
    EXPECT_TRUE(execute("position startpos moves e7e5"));

    EXPECT_EQ(board().to_fen(), board_test::fen::start);
    EXPECT_NE(output.str().find("error: invalid move in position command: e7e5"), std::string::npos)
        << output.str();
}

TEST_F(EnginePositionTest, FailedPositionCommandsPreserveBoardAndHistory) {
    struct FailureCase {
        std::string_view command;
        std::string_view output;
    };

    const std::string     seed       = std::format("position fen {} moves "
                                                   "e6f5 h7g8 f5e6 g8h7 e6f5 h7g8 f5e6 g8h7 e6f5",
                                         board_test::fen::repetition_cycle);
    constexpr FailureCase failures[] = {
        {"position startpos moves e2e4 invalid",
         "info string error: invalid move in position command: invalid\n"},
        {"position startpos moves e2e4 e7e5 e4e5",
         "info string error: invalid move in position command: e4e5\n"},
        {"position fen invalid", "info string error: invalid fen, must have 4 or 6 fields\n"},
        {"position abc", "info string error: invalid position command\n"},
    };

    for (const auto& failure : failures) {
        SCOPED_TRACE(failure.command);
        ASSERT_TRUE(execute(seed));
        ASSERT_TRUE(board().is_draw());
        const Board before{board()};

        output.str("");
        output.clear();
        EXPECT_TRUE(execute(std::string(failure.command)));

        expect_same_board_and_history(board(), before);
        EXPECT_EQ(output.str(), failure.output);
    }
}

TEST_F(EnginePositionTest, MovesCommandFiltersIllegalPseudoLegalMoves) {
    EXPECT_TRUE(execute(std::format("position fen {}", board_test::fen::pinned_rook)));
    ASSERT_EQ(board().to_fen(), board_test::fen::pinned_rook);
    output.str("");
    output.clear();

    EXPECT_TRUE(execute("moves"));

    EXPECT_NE(output.str().find("e2e8"), std::string::npos) << output.str();
    EXPECT_EQ(output.str().find("e2a2"), std::string::npos) << output.str();
}

TEST_F(EnginePositionTest, MoveCommandReportsInvalidMoveToken) {
    EXPECT_TRUE(execute("move notamove"));

    EXPECT_NE(output.str().find("invalid move: notamove"), std::string::npos) << output.str();
}

struct PositionCase {
    std::string cmd;
    std::string fen;
};

class PositionTest : public EnginePositionTest,
                     public ::testing::WithParamInterface<PositionCase> {};

TEST_P(PositionTest, ValidatePosition) {
    const auto& param = GetParam();

    // Execute the position command
    EXPECT_TRUE(execute(param.cmd));

    // Check if the board is set to the expected FEN
    EXPECT_EQ(board().to_fen(), param.fen);
}

INSTANTIATE_TEST_SUITE_P(
    PositionTests,
    PositionTest,
    ::testing::Values(
        PositionCase{.cmd = "position", .fen = board_test::fen::start},
        PositionCase{.cmd = "position abc", .fen = board_test::fen::start},
        PositionCase{.cmd = "position startpos", .fen = board_test::fen::start},
        PositionCase{.cmd = "position startpos moves", .fen = board_test::fen::start},
        PositionCase{.cmd = "position startpos moves e2e4", .fen = board_test::fen::after_e2e4},
        PositionCase{.cmd = "position startpos moves e7e5", .fen = board_test::fen::start},
        PositionCase{.cmd = std::format("position fen {}", board_test::fen::kings_only),
                     .fen = board_test::fen::kings_only},
        PositionCase{.cmd = std::format("position fen {} abc", board_test::fen::kings_only),
                     .fen = board_test::fen::start},
        PositionCase{.cmd = std::format("position fen {} moves", board_test::fen::kings_only),
                     .fen = board_test::fen::kings_only},
        PositionCase{.cmd = std::format("position fen {} moves abc", board_test::fen::kings_only),
                     .fen = board_test::fen::start},
        PositionCase{.cmd = std::format("position fen {} moves a1b1", board_test::fen::kings_only),
                     .fen = board_test::fen::start},
        PositionCase{.cmd = std::format("position fen {} moves e1e2", board_test::fen::kings_only),
                     .fen = "4k3/8/8/8/8/8/4K3/8 b - - 1 1"}));
