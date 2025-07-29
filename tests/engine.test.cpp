#include "engine.hpp"

#include <format>
#include <sstream>
#include <thread>

#include "test_util.hpp"
#include "gtest/gtest.h"

class EngineTest : public ::testing::Test {
protected:
    std::ostringstream output;
    Engine             engine{output, output, std::cin};

    void SetUp() override { output.str(""); }

    bool        execute(const std::string& command) { return engine.execute(command); }
    Board&      board() { return engine.board; }
    ThreadPool& threadpool() { return engine.thread_pool; }
};

TEST_F(EngineTest, GoAndStopCommands) {
    EXPECT_TRUE(execute("go"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(execute("stop"));
    threadpool().wait_all();
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

TEST_F(EngineTest, ExitCommand) {
    EXPECT_FALSE(execute("exit"));
}

TEST_F(EngineTest, QuitCommand) {
    EXPECT_FALSE(execute("quit"));
}

// Basic engine command tests

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
    EXPECT_EQ(board().toFEN(), param.expected_fen);
    EXPECT_NE(output.str().find(param.expected_output), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    EngineCommandsTests,
    EngineCommandsTest,
    ::testing::Values(CommandCase{{"uci"}, STARTFEN, "id name Latrunculi"},
                      CommandCase{{"invalidcommand"}, STARTFEN, "unknown command"},
                      CommandCase{{"isready"}, STARTFEN, "readyok"},
                      CommandCase{{"ucinewgame"}, STARTFEN, ""},
                      CommandCase{{"debug on"}, STARTFEN, ""},
                      CommandCase{{"debug off"}, STARTFEN, ""},
                      CommandCase{{"ponderhit"}, STARTFEN, ""},
                      CommandCase{{"position startpos", "move e2e4"}, E2E4, ""},
                      CommandCase{{"position startpos", "move e2e4", "move undo"}, STARTFEN, ""},
                      CommandCase{{"position startpos", "moves"}, STARTFEN, "e2e4"},
                      CommandCase{{"position startpos", "perft 1"}, STARTFEN, "NODES: 20"}));

// setoption tests

struct SetOptionCase {
    std::string command;
    int         threads = DEFAULT_THREADS;
    std::string output  = "error";
};

class SetOptionTest : public EngineTest, public ::testing::WithParamInterface<SetOptionCase> {};

TEST_P(SetOptionTest, ValidateSetOption) {
    const auto& param = GetParam();

    EXPECT_TRUE(execute(param.command));
    EXPECT_EQ(threadpool().size(), param.threads);
    EXPECT_NE(output.str().find(param.output), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    SetOptionTests,
    SetOptionTest,
    ::testing::Values(
        SetOptionCase{.command = "setoption"},
        SetOptionCase{.command = "setoption abc"},
        SetOptionCase{.command = "setoption name"},
        SetOptionCase{.command = "setoption name abc"},
        SetOptionCase{.command = "setoption name Threads"},
        SetOptionCase{.command = "setoption name Threads abc"},
        SetOptionCase{.command = "setoption name Threads value"},
        SetOptionCase{.command = "setoption name Threads value abc"},
        SetOptionCase{.command = "setoption name Threads value -1"},
        SetOptionCase{.command = "setoption name Threads value 0"},
        SetOptionCase{.command = "setoption name Threads value 99999"},
        SetOptionCase{.command = "setoption name Threads value 4", .threads = 4, .output = ""}));

// position command tests

struct PositionCase {
    std::string cmd;
    std::string fen;
};

class PositionTest : public EngineTest, public ::testing::WithParamInterface<PositionCase> {};

TEST_P(PositionTest, ValidatePosition) {
    const auto& param = GetParam();
    EXPECT_TRUE(execute(param.cmd));
    EXPECT_EQ(board().toFEN(), param.fen);
}

INSTANTIATE_TEST_SUITE_P(
    PositionTests,
    PositionTest,
    ::testing::Values(
        PositionCase{.cmd = "position", .fen = STARTFEN},
        PositionCase{.cmd = "position abc", .fen = STARTFEN},
        PositionCase{.cmd = "position startpos", .fen = STARTFEN},
        PositionCase{.cmd = "position startpos moves", .fen = STARTFEN},
        PositionCase{.cmd = "position startpos moves e2e4", .fen = E2E4},
        PositionCase{.cmd = "position startpos moves e7e5", .fen = STARTFEN},
        PositionCase{.cmd = std::format("position fen {}", EMPTYFEN), .fen = EMPTYFEN},
        PositionCase{.cmd = std::format("position fen {} abc", EMPTYFEN), .fen = EMPTYFEN},
        PositionCase{.cmd = std::format("position fen {} moves", EMPTYFEN), .fen = EMPTYFEN},
        PositionCase{.cmd = std::format("position fen {} moves abc", EMPTYFEN), .fen = EMPTYFEN},
        PositionCase{.cmd = std::format("position fen {} moves a1b1", EMPTYFEN), .fen = EMPTYFEN},
        PositionCase{.cmd = std::format("position fen {} moves e1e2", EMPTYFEN),
                     .fen = "4k3/8/8/8/8/8/4K3/8 b - - 1 1"}));

// go command tests

struct GoCase {
    std::string command;
    std::string output;
};

class GoTest : public EngineTest, public ::testing::WithParamInterface<GoCase> {};

TEST_P(GoTest, ValidateOutput) {
    const auto& param = GetParam();

    EXPECT_TRUE(execute(param.command));
    threadpool().wait_all();
    EXPECT_NE(output.str().find(param.output), std::string::npos) << output.str();
}

INSTANTIATE_TEST_SUITE_P(GoTests,
                         GoTest,
                         ::testing::Values(GoCase{"go depth 3", "bestmove"},
                                           GoCase{"go movetime 50", "bestmove"},
                                           GoCase{"go nodes 1000", "bestmove"},
                                           GoCase{"go wtime 1000 btime 1000", "bestmove"},
                                           GoCase{"go wtime 1000 btime 1000 winc 100 binc 100",
                                                  "bestmove"}));
