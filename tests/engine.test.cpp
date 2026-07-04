#include "engine.hpp"

#include <format>
#include <sstream>
#include <string_view>
#include <thread>

#include "test_util.hpp"
#include "tt.hpp"
#include "gtest/gtest.h"

class EngineTest : public ::testing::Test {
protected:
    std::ostringstream output;
    Engine             engine{output, output, std::cin};

    void SetUp() override {
        output.str("");
        output.clear();
        tt.clear();
    }

    bool        execute(const std::string& command) { return engine.execute(command); }
    Board&      board() { return engine.board; }
    ThreadPool& threadpool() { return engine.thread_pool; }

    int count_output_lines_starting_with(std::string_view prefix) const {
        std::istringstream lines{output.str()};
        std::string        line;
        int                count = 0;

        while (std::getline(lines, line)) {
            if (line.starts_with(prefix))
                ++count;
        }

        return count;
    }

    bool wait_for_busy(std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (threadpool().is_searching())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return threadpool().is_searching();
    }
};

TEST(EngineLoopTest, ReadsConfiguredInputStream) {
    std::istringstream input{"isready\nquit\n"};
    std::ostringstream output;
    Engine             engine{output, output, input};

    engine.loop();

    EXPECT_NE(output.str().find("readyok"), std::string::npos);
}

TEST_F(EngineTest, GoAndStopCommands) {
    // Start a search
    EXPECT_TRUE(execute("go"));

    // Wait for a short time and stop the search
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(execute("stop"));

    // Wait for threads to finish and check output for best move
    threadpool().wait();
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

TEST_F(EngineTest, GoDepthReportsEveryCompletedDepthBeforeBestmove) {
    EXPECT_TRUE(execute("go depth 3"));
    threadpool().wait();

    const std::string transcript = output.str();
    EXPECT_GE(count_output_lines_starting_with("info depth 1 "), 1) << transcript;
    EXPECT_GE(count_output_lines_starting_with("info depth 2 "), 1) << transcript;
    EXPECT_GE(count_output_lines_starting_with("info depth 3 "), 1) << transcript;
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1) << transcript;

    const auto depth1   = transcript.find("info depth 1 ");
    const auto depth2   = transcript.find("info depth 2 ");
    const auto depth3   = transcript.find("info depth 3 ");
    const auto bestmove = transcript.find("bestmove ");

    ASSERT_NE(depth1, std::string::npos) << transcript;
    ASSERT_NE(depth2, std::string::npos) << transcript;
    ASSERT_NE(depth3, std::string::npos) << transcript;
    ASSERT_NE(bestmove, std::string::npos) << transcript;
    EXPECT_LT(depth1, depth2) << transcript;
    EXPECT_LT(depth2, depth3) << transcript;
    EXPECT_LT(depth3, bestmove) << transcript;
}

TEST_F(EngineTest, GoWhileSearchInProgressIsRejected) {
    EXPECT_TRUE(execute("go"));
    ASSERT_TRUE(wait_for_busy());

    output.str("");
    output.clear();

    EXPECT_TRUE(execute("go depth 1"));
    EXPECT_NE(output.str().find("search already in progress"), std::string::npos) << output.str();

    EXPECT_TRUE(execute("stop"));
    threadpool().wait();
}

TEST_F(EngineTest, SetOptionWhileSearchInProgressIsRejected) {
    EXPECT_TRUE(execute("go"));
    ASSERT_TRUE(wait_for_busy());

    output.str("");
    output.clear();

    EXPECT_TRUE(execute("setoption name Threads value 2"));
    EXPECT_NE(output.str().find("cannot set option while search is in progress"), std::string::npos)
        << output.str();
    EXPECT_EQ(threadpool().thread_count(), DEFAULT_THREADS);

    EXPECT_TRUE(execute("stop"));
    threadpool().wait();
}

TEST_F(EngineTest, UciNewGameWhileSearchInProgressIsRejected) {
    EXPECT_TRUE(execute("go"));
    ASSERT_TRUE(wait_for_busy());

    output.str("");
    output.clear();

    EXPECT_TRUE(execute("ucinewgame"));
    EXPECT_NE(output.str().find("cannot start new game while search is in progress"),
              std::string::npos)
        << output.str();

    EXPECT_TRUE(execute("stop"));
    threadpool().wait();
}

TEST_F(EngineTest, ExitCommand) {
    EXPECT_FALSE(execute("exit"));
}

TEST_F(EngineTest, QuitCommand) {
    EXPECT_FALSE(execute("quit"));
}

TEST_F(EngineTest, SearchDoesNotReuseStaleBestMoveWhenNoLegalMoves) {
    // Seed an existing bestmove.
    EXPECT_TRUE(execute("position startpos"));
    EXPECT_TRUE(execute("go depth 1"));
    threadpool().wait();

    // Run a checkmated position; bestmove must not reuse the prior move.
    output.str("");
    output.clear();

    EXPECT_TRUE(execute("position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"));
    EXPECT_TRUE(execute("go depth 1"));
    threadpool().wait();

    EXPECT_NE(output.str().find("bestmove none"), std::string::npos) << output.str();
    EXPECT_EQ(output.str().find("bestmove e2e4"), std::string::npos) << output.str();
}

TEST_F(EngineTest, UciNewGameClearsTTAndResetsGeneration) {
    tt.age_table();
    tt.store(board().key(), Move(Square::E2, Square::E4), 42, 3, TT_Flag::Exact, 0);
    ASSERT_TRUE(tt.probe(board().key()).has_value());
    ASSERT_EQ(tt.current_age(), uint8_t{1});

    EXPECT_TRUE(execute("ucinewgame"));

    EXPECT_FALSE(tt.probe(board().key()).has_value());
    EXPECT_EQ(tt.current_age(), uint8_t{0});
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

TEST_F(EngineTest, PositionReportsInvalidMoveToken) {
    EXPECT_TRUE(execute("position startpos moves e7e5"));

    EXPECT_EQ(board().toFEN(), STARTFEN);
    EXPECT_NE(output.str().find("error: invalid move in position command: e7e5"), std::string::npos)
        << output.str();
}

TEST_F(EngineTest, MovesCommandFiltersIllegalPseudoLegalMoves) {
    EXPECT_TRUE(execute("position fen k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1"));
    ASSERT_EQ(board().toFEN(), "k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    output.str("");
    output.clear();

    EXPECT_TRUE(execute("moves"));

    EXPECT_NE(output.str().find("e2e8"), std::string::npos) << output.str();
    EXPECT_EQ(output.str().find("e2a2"), std::string::npos) << output.str();
}

// setoption tests

struct SetOptionCase {
    std::string command;
    int         threads = DEFAULT_THREADS;
    std::string output  = "error";
};

class SetOptionTest : public EngineTest, public ::testing::WithParamInterface<SetOptionCase> {};

TEST_P(SetOptionTest, ValidateSetOption) {
    const auto& param = GetParam();

    // Execute the setoption command
    EXPECT_TRUE(execute(param.command));

    // Check if the thread count is set correctly and output is as expected
    EXPECT_EQ(threadpool().thread_count(), param.threads);
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
        SetOptionCase{.command = "setoption name Threads value 2 extra"},
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

    // Execute the position command
    EXPECT_TRUE(execute(param.cmd));

    // Check if the board is set to the expected FEN
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
        PositionCase{.cmd = std::format("position fen {} abc", EMPTYFEN), .fen = STARTFEN},
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

    // Start the search
    EXPECT_TRUE(execute(param.command));

    // Wait for the search to complete and check output
    threadpool().wait();
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
