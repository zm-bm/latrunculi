#include "support/engine_test_fixture.hpp"

#include <format>
#include <sstream>
#include <string>
#include <vector>

#include "support/board_fixtures.hpp"
#include "gtest/gtest.h"

TEST(EngineLoopTest, ReadsConfiguredInputStream) {
    std::istringstream input{"isready\nquit\n"};
    std::ostringstream output;
    uci::Engine        engine{output, output, input};

    engine.loop();

    EXPECT_NE(output.str().find("readyok"), std::string::npos);
}

TEST_F(EngineTest, ExitCommand) {
    EXPECT_FALSE(execute("exit"));
}

TEST_F(EngineTest, QuitCommand) {
    EXPECT_FALSE(execute("quit"));
}

TEST_F(EngineTest, RegisterCommandIsSilentNoop) {
    EXPECT_TRUE(execute("register later"));

    EXPECT_TRUE(output.str().empty()) << output.str();
}

TEST_F(EngineTest, UnknownInputIsIgnoredAndRecoveredDebugControlsDiagnostics) {
    EXPECT_TRUE(execute("invalidcommand"));
    EXPECT_TRUE(output.str().empty()) << output.str();

    EXPECT_TRUE(execute("joho debug on"));
    EXPECT_EQ(output.str(), "info string debug mode enabled\n");

    output.str("");
    output.clear();
    EXPECT_TRUE(execute("position startpos moves e2e4"));
    EXPECT_EQ(output.str(),
              std::format("info string debug position {}\n", board_test::fen::after_e2e4));

    output.str("");
    output.clear();
    EXPECT_TRUE(execute("debug off"));
    EXPECT_EQ(output.str(), "info string debug mode disabled\n");

    output.str("");
    output.clear();
    EXPECT_TRUE(execute("position startpos"));
    EXPECT_TRUE(output.str().empty()) << output.str();
}

struct CommandCase {
    std::vector<std::string> commands;
    std::string              expected_fen;
    std::string              expected_output;
};

class EngineCommandsTest : public EngineTest, public ::testing::WithParamInterface<CommandCase> {};

TEST_P(EngineCommandsTest, ValidateCommands) {
    const auto& param = GetParam();

    for (const auto& cmd : param.commands)
        EXPECT_TRUE(execute(cmd));
    EXPECT_EQ(board().to_fen(), param.expected_fen);
    EXPECT_NE(output.str().find(param.expected_output), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    EngineCommandsTests,
    EngineCommandsTest,
    ::testing::Values(
        CommandCase{{"uci"}, board_test::fen::start, "id name Latrunculi"},
        CommandCase{{"invalidcommand"}, board_test::fen::start, ""},
        CommandCase{{"isready"}, board_test::fen::start, "readyok"},
        CommandCase{{"ucinewgame"}, board_test::fen::start, ""},
        CommandCase{{"position startpos", "move e2e4"}, board_test::fen::after_e2e4, ""},
        CommandCase{{"position startpos", "move e2e4", "move undo"}, board_test::fen::start, ""},
        CommandCase{{"position startpos", "moves"}, board_test::fen::start, "e2e4"},
        CommandCase{{"position startpos", "perft 1"}, board_test::fen::start, "NODES: 20"},
        CommandCase{{"position startpos", "perft 0"}, board_test::fen::start, "NODES: 1"}));
