#include "engine.hpp"

#include <sstream>
#include <thread>

#include "gtest/gtest.h"
#include "tt.hpp"

using namespace std::literals;

const auto E2E4 = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";

class EngineTest : public ::testing::Test {
   protected:
    std::ostringstream output;
    Engine engine{output, std::cin};

    void SetUp() override {
        output.str("");  // Clear output before each test
    }

    bool execute(const std::string& command) { return engine.execute(command); }

    Board& board() { return engine.board; }
    ThreadPool& threadpool() { return engine.threadpool; }
};

TEST_F(EngineTest, StopCommand) {
    EXPECT_TRUE(execute("go"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(execute("stop"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_NE(output.str().find("bestmove"), std::string::npos);
}

// --------------------------
// UCI command tests
// --------------------------

struct UCICase {
    std::vector<std::string> commands;    // commands to run in sequence
    std::string expectedBoard;            // if non-empty then an expected board FEN
    std::string expectedOutputSubstring;  // if non-empty then an expected substring in output
};

class UCICommandParameterizedTest : public EngineTest,
                                    public ::testing::WithParamInterface<UCICase> {};

TEST_P(UCICommandParameterizedTest, ValidateCommandSequence) {
    const auto& param = GetParam();

    for (const auto& cmd : param.commands) {
        EXPECT_TRUE(execute(cmd));
    }
    if (!param.expectedBoard.empty()) {
        EXPECT_EQ(board().toFEN(), param.expectedBoard);
    }
    if (!param.expectedOutputSubstring.empty()) {
        EXPECT_NE(output.str().find(param.expectedOutputSubstring), std::string::npos);
    }
}

INSTANTIATE_TEST_SUITE_P(
    UCICommandTests,
    UCICommandParameterizedTest,
    ::testing::Values(UCICase{{"uci"}, "", "id name Latrunculi"},
                      UCICase{{"invalidcommand"}, "", "unknown command"},
                      UCICase{{"isready"}, "", "readyok"},
                      UCICase{{"ucinewgame"}, "", ""},
                      UCICase{{"uci", "quit"}, "", ""},
                      UCICase{{"uci", "exit"}, "", ""},
                      UCICase{{"debug on"}, "", ""},
                      UCICase{{"debug off"}, "", ""},
                      UCICase{{"ponderhit"}, "", ""},
                      UCICase{{"position startpos", "move e2e4"}, E2E4, ""},
                      UCICase{{"position startpos", "move e2e4", "move undo"}, STARTFEN, ""},
                      UCICase{{"position startpos", "moves"}, "", "e2e4"},
                      UCICase{{"position startpos", "perft 1"}, "", "NODES: 20"}));

// --------------------------
// setoption name Threads tests
// --------------------------

struct ThreadsCase {
    std::string command;
    int expectedPoolSize;
    std::string expectedOutputSubstring;  // if non-empty then an expected substring in output
};

class SetOptionThreadsParameterizedTest : public EngineTest,
                                          public ::testing::WithParamInterface<ThreadsCase> {};

TEST_P(SetOptionThreadsParameterizedTest, ValidateThreadsOption) {
    const auto& param = GetParam();
    EXPECT_TRUE(execute(param.command));
    EXPECT_EQ(threadpool().size(), param.expectedPoolSize);
    if (!param.expectedOutputSubstring.empty())
        EXPECT_NE(output.str().find(param.expectedOutputSubstring), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    SetOptionThreadsTests,
    SetOptionThreadsParameterizedTest,
    ::testing::Values(
        ThreadsCase{"setoption name Threads value abc", DEFAULT_THREADS, "invalid setoption"},
        ThreadsCase{"setoption name Threads value -1", DEFAULT_THREADS, "invalid setoption"},
        ThreadsCase{"setoption name Threads value 0", DEFAULT_THREADS, "invalid setoption"},
        ThreadsCase{"setoption name Threads value " + std::to_string(MAX_THREADS + 1),
                    DEFAULT_THREADS,
                    "invalid setoption"},
        ThreadsCase{"setoption name Threads value 4", 4, ""}));

// --------------------------
// setoption name Hash tests
// --------------------------

struct HashCase {
    std::string command;
    int expectedHashSize;
    std::string expectedOutputSubstring;  // if non-empty then an expected substring in output
};

class SetOptionHashParameterizedTest : public EngineTest,
                                       public ::testing::WithParamInterface<HashCase> {
   protected:
    void SetUp() override { TT::table.resize(DEFAULT_HASH_MB); }
};

TEST_P(SetOptionHashParameterizedTest, ValidateHashOption) {
    const auto& param = GetParam();
    EXPECT_TRUE(execute(param.command));
    EXPECT_EQ(TT::table.size(), param.expectedHashSize);
    if (!param.expectedOutputSubstring.empty())
        EXPECT_NE(output.str().find(param.expectedOutputSubstring), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    SetOptionHashTests,
    SetOptionHashParameterizedTest,
    ::testing::Values(
        HashCase{"setoption name Hash value abc", DEFAULT_HASH_MB, "invalid setoption"},
        HashCase{"setoption name Hash value -1", DEFAULT_HASH_MB, "invalid setoption"},
        HashCase{"setoption name Hash value 0", DEFAULT_HASH_MB, "invalid setoption"},
        HashCase{"setoption name Hash value "s + std::to_string(MAX_HASH_MB + 1),
                 DEFAULT_HASH_MB,
                 "invalid setoption"},
        HashCase{"setoption name Hash value 64", 64, ""}));

// --------------------------
// setoption invalid commands tests
// --------------------------

struct SetOptionCase {
    std::string command;
};

class SetOptionInvalidParameterizedTest : public EngineTest,
                                          public ::testing::WithParamInterface<SetOptionCase> {};

TEST_P(SetOptionInvalidParameterizedTest, ValidateInvalidOption) {
    const auto& param = GetParam();
    EXPECT_TRUE(execute(param.command));
    EXPECT_NE(output.str().find("invalid setoption"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(SetOptionInvalidTests,
                         SetOptionInvalidParameterizedTest,
                         ::testing::Values(SetOptionCase{"setoption"},
                                           SetOptionCase{"setoption abc"},
                                           SetOptionCase{"setoption name"},
                                           SetOptionCase{"setoption name abc"},
                                           SetOptionCase{"setoption name abc value"},
                                           SetOptionCase{"setoption name abc value abc"},
                                           SetOptionCase{"setoption name abc value 10"}));

// --------------------------
// position command tests
// --------------------------

struct PositionCase {
    std::string command;
    std::string expectedFEN;     // if non-empty then an expected board state
    std::string expectedOutput;  // if non-empty then an expected error message
};

class PositionParameterizedTest : public EngineTest,
                                  public ::testing::WithParamInterface<PositionCase> {};

TEST_P(PositionParameterizedTest, ValidatePosition) {
    const auto& param = GetParam();
    EXPECT_TRUE(execute(param.command));
    if (!param.expectedFEN.empty()) {
        EXPECT_EQ(board().toFEN(), param.expectedFEN);
    } else {
        EXPECT_NE(output.str().find(param.expectedOutput), std::string::npos);
    }
}

INSTANTIATE_TEST_SUITE_P(
    PositionTests,
    PositionParameterizedTest,
    ::testing::Values(PositionCase{"position", "", "invalid position"},
                      PositionCase{"position abc", "", "invalid position"},
                      PositionCase{"position startpos", STARTFEN, ""},
                      PositionCase{"position startpos moves", STARTFEN, ""},
                      PositionCase{"position startpos moves e2e4", E2E4, ""},
                      PositionCase{"position startpos moves e7e5", STARTFEN, ""},
                      PositionCase{"position fen "s + EMPTYFEN, EMPTYFEN, ""},
                      PositionCase{"position fen "s + EMPTYFEN + " abc", EMPTYFEN, ""},
                      PositionCase{"position fen "s + EMPTYFEN + " moves", EMPTYFEN, ""},
                      PositionCase{"position fen "s + EMPTYFEN + " moves abc", EMPTYFEN, ""},
                      PositionCase{"position fen "s + EMPTYFEN + " moves e1e2 e8d8",
                                   "3k4/8/8/8/8/8/4K3/8 w - - 2 2",
                                   ""}));

// --------------------------
// go command tests
// --------------------------

struct GoCommandTestCase {
    std::string command;
    std::string expectedSubstring;
    int sleepMilliseconds;
};

class GoCommandParameterizedTest : public EngineTest,
                                   public ::testing::WithParamInterface<GoCommandTestCase> {};

TEST_P(GoCommandParameterizedTest, ValidateOutput) {
    const auto& param = GetParam();
    EXPECT_TRUE(execute(param.command));
    if (param.sleepMilliseconds > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(param.sleepMilliseconds));
    EXPECT_NE(output.str().find(param.expectedSubstring), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidGoCommands,
    GoCommandParameterizedTest,
    ::testing::Values(GoCommandTestCase{"go depth -3", "invalid depth", 0},
                      GoCommandTestCase{"go depth abc", "invalid depth", 0},
                      GoCommandTestCase{"go movetime -1000", "invalid movetime", 0},
                      GoCommandTestCase{"go movetime abc", "invalid movetime", 0},
                      GoCommandTestCase{"go nodes -10000", "invalid nodes", 0},
                      GoCommandTestCase{"go nodes abc", "invalid nodes", 0}));

INSTANTIATE_TEST_SUITE_P(
    ValidGoCommands,
    GoCommandParameterizedTest,
    ::testing::Values(GoCommandTestCase{"go depth 3", "bestmove", 50},
                      GoCommandTestCase{"go movetime 50", "bestmove", 150},
                      GoCommandTestCase{"go nodes 10000", "bestmove", 150},
                      GoCommandTestCase{"go wtime 1000 btime 1000", "bestmove", 150},
                      GoCommandTestCase{
                          "go wtime 1000 btime 1000 winc 100 binc 100", "bestmove", 200}));
