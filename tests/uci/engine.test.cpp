#include "uci/engine.hpp"

#include <cstdint>
#include <format>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include "board/board.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_snapshot.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"
#include "gtest/gtest.h"

class EngineTest : public ::testing::Test {
protected:
    std::ostringstream output;
    uci::Engine        engine{output, output, std::cin};

    void SetUp() override {
        output.str("");
        output.clear();
        tt.clear();
    }

    bool              execute(const std::string& command) { return engine.execute(command); }
    Board&            board() { return engine.board; }
    SearchThreadPool& thread_pool() { return engine.thread_pool; }
    int               hash_option_mb() const { return engine.options.hash.value; }
    int               thread_option_count() const { return engine.options.threads.value; }
    bool              ponder_enabled() const { return engine.options.ponder.value; }

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
            if (thread_pool().is_searching())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return thread_pool().is_searching();
    }

    bool wait_for_depth(int                       depth,
                        std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
        SearchWorker& worker   = SearchThreadTestAccess::worker(thread_pool());
        const auto    deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (worker.root_snapshot().depth >= depth)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return worker.root_snapshot().depth >= depth;
    }
};

TEST(EngineLoopTest, ReadsConfiguredInputStream) {
    std::istringstream input{"isready\nquit\n"};
    std::ostringstream output;
    uci::Engine        engine{output, output, input};

    engine.loop();

    EXPECT_NE(output.str().find("readyok"), std::string::npos);
}

TEST_F(EngineTest, ImmediateStopReportsLegalMove) {
    for (const std::string_view command : {"go", "go ponder"}) {
        SCOPED_TRACE(command);
        output.str("");
        output.clear();

        EXPECT_TRUE(execute(std::string(command)));
        EXPECT_TRUE(execute("stop"));
        thread_pool().wait();

        const std::string transcript = output.str();
        EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1) << transcript;
        EXPECT_EQ(transcript.find("bestmove 0000"), std::string::npos) << transcript;
    }
}

TEST_F(EngineTest, InfiniteSearchWaitsForStop) {
    EXPECT_TRUE(execute("go infinite depth 1 nodes 0"));

    ASSERT_TRUE(wait_for_depth(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(thread_pool().is_searching());
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 0);

    EXPECT_TRUE(execute("stop"));
    thread_pool().wait();

    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1) << output.str();
}

TEST_F(EngineTest, GoDepthReportsFreshFinalInformationBeforeBestmove) {
    ASSERT_TRUE(execute("setoption name Threads value 2"));
    EXPECT_TRUE(execute("go depth 3"));
    thread_pool().wait();

    const std::string transcript = output.str();
    EXPECT_GE(count_output_lines_starting_with("info depth 1 "), 1) << transcript;
    EXPECT_GE(count_output_lines_starting_with("info depth 2 "), 1) << transcript;
    EXPECT_GE(count_output_lines_starting_with("info depth 3 "), 1) << transcript;
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1) << transcript;

    const auto depth1     = transcript.find("info depth 1 ");
    const auto depth2     = transcript.find("info depth 2 ");
    const auto depth3     = transcript.find("info depth 3 ");
    const auto bestmove   = transcript.find("bestmove ");
    const auto final_info = transcript.rfind("info depth ", bestmove);

    ASSERT_NE(depth1, std::string::npos) << transcript;
    ASSERT_NE(depth2, std::string::npos) << transcript;
    ASSERT_NE(depth3, std::string::npos) << transcript;
    ASSERT_NE(bestmove, std::string::npos) << transcript;
    ASSERT_NE(final_info, std::string::npos) << transcript;
    EXPECT_LT(depth1, depth2) << transcript;
    EXPECT_LT(depth2, depth3) << transcript;
    EXPECT_LT(depth3, bestmove) << transcript;

    const auto final_info_end = transcript.find('\n', final_info);
    ASSERT_NE(final_info_end, std::string::npos) << transcript;
    EXPECT_EQ(final_info_end + 1, bestmove) << transcript;

    const auto nodes_begin = transcript.find(" nodes ", final_info);
    ASSERT_NE(nodes_begin, std::string::npos) << transcript;
    const auto nodes_value = nodes_begin + std::string_view{" nodes "}.size();
    const auto nodes_end   = transcript.find(' ', nodes_value);
    ASSERT_NE(nodes_end, std::string::npos) << transcript;
    EXPECT_EQ(std::stoull(transcript.substr(nodes_value, nodes_end - nodes_value)),
              thread_pool().nodes_searched())
        << transcript;
}

TEST_F(EngineTest, GoPreservesWideNodeLimit) {
    constexpr NodeCount node_limit = static_cast<NodeCount>(std::numeric_limits<int>::max()) + 1;

    EXPECT_TRUE(execute(std::format("go depth 1 nodes {}", node_limit)));
    thread_pool().wait();

    const auto& limits = SearchTestAccess::limits(SearchThreadTestAccess::worker(thread_pool()));
    EXPECT_EQ(limits.nodes, node_limit);
}

TEST_F(EngineTest, SearchmovesRestrictAndValidateRootMoves) {
    EXPECT_TRUE(execute("go depth 1 searchmoves a2a3 a2a3"));
    thread_pool().wait();
    EXPECT_NE(output.str().find("bestmove a2a3"), std::string::npos) << output.str();

    const std::vector expected_moves{Move(A2, A3)};
    const auto& limits = SearchTestAccess::limits(SearchThreadTestAccess::worker(thread_pool()));
    EXPECT_EQ(limits.root_moves, expected_moves);

    struct InvalidCase {
        const char* command;
        const char* error;
    };

    constexpr InvalidCase invalid_cases[] = {
        {"go searchmoves", "missing searchmoves"},
        {"go searchmoves e2e4 e7e5", "invalid searchmove: e7e5"},
    };

    for (const auto& tc : invalid_cases) {
        SCOPED_TRACE(tc.command);
        output.str("");
        output.clear();

        EXPECT_TRUE(execute(tc.command));

        EXPECT_FALSE(thread_pool().is_searching());
        EXPECT_EQ(output.str(), std::format("info string error: {}\n", tc.error));
    }
}

TEST_F(EngineTest, GoWhileSearchInProgressIsRejected) {
    EXPECT_TRUE(execute("go"));
    ASSERT_TRUE(wait_for_busy());

    EXPECT_TRUE(execute("go searchmoves invalid"));
    EXPECT_TRUE(execute("stop"));
    thread_pool().wait();

    EXPECT_NE(output.str().find("search already in progress"), std::string::npos) << output.str();
    EXPECT_EQ(output.str().find("invalid searchmove"), std::string::npos) << output.str();
}

TEST_F(EngineTest, StateChangingCommandsWhileSearchInProgressAreRejected) {
    EXPECT_TRUE(execute("go infinite depth 1"));
    ASSERT_TRUE(wait_for_busy());

    EXPECT_TRUE(execute("setoption name Threads value 2"));
    EXPECT_TRUE(execute("ucinewgame"));
    EXPECT_TRUE(execute("position startpos moves e2e4"));
    EXPECT_TRUE(execute("move d2d4"));

    EXPECT_EQ(thread_pool().thread_count(), uci::Options::default_threads);
    EXPECT_EQ(board().to_fen(), board_test::fen::start);
    EXPECT_TRUE(thread_pool().is_searching());

    EXPECT_TRUE(execute("stop"));
    thread_pool().wait();

    const std::string transcript = output.str();
    for (const std::string_view message : {
             "cannot set option while search is in progress",
             "cannot start new game while search is in progress",
             "cannot set position while search is in progress",
             "cannot run console command while search is in progress",
         })
        EXPECT_NE(transcript.find(message), std::string::npos) << transcript;
}

TEST_F(EngineTest, ExitCommand) {
    EXPECT_FALSE(execute("exit"));
}

TEST_F(EngineTest, QuitCommand) {
    EXPECT_FALSE(execute("quit"));
}

TEST_F(EngineTest, TerminalPositionsReportDepthZeroAndNullBestMove) {
    // Seed an existing bestmove.
    EXPECT_TRUE(execute("position startpos"));
    EXPECT_TRUE(execute("go depth 1"));
    thread_pool().wait();

    constexpr std::string_view terminal_positions[] = {
        "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1",
        board_test::fen::stalemate,
    };

    for (const std::string_view fen : terminal_positions) {
        SCOPED_TRACE(fen);
        output.str("");
        output.clear();

        EXPECT_TRUE(execute(std::format("position fen {}", fen)));
        EXPECT_TRUE(execute("go depth 10"));
        thread_pool().wait();

        const std::string transcript = output.str();
        EXPECT_EQ(count_output_lines_starting_with("info depth 0 "), 1) << transcript;
        EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1) << transcript;
        EXPECT_NE(transcript.find("bestmove 0000"), std::string::npos) << transcript;
    }
}

TEST_F(EngineTest, PositionStartposResetsFromNonStartPosition) {
    EXPECT_TRUE(execute(std::format("position fen {}", board_test::fen::kings_only)));
    ASSERT_EQ(board().to_fen(), board_test::fen::kings_only);

    EXPECT_TRUE(execute("position startpos"));

    EXPECT_EQ(board().to_fen(), board_test::fen::start);
}

TEST_F(EngineTest, PositionMovesBuildUndoableHistory) {
    EXPECT_TRUE(execute("position startpos moves e2e4 e7e5"));
    ASSERT_TRUE(board().can_unmake());

    board().unmake();
    EXPECT_EQ(board().to_fen(), board_test::fen::after_e2e4);

    board().unmake();
    EXPECT_FALSE(board().can_unmake());
    EXPECT_EQ(board().to_fen(), board_test::fen::start);
}

TEST_F(EngineTest, PositionMovesBuildRepetitionHistoryThatFenReplacementClears) {
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

TEST_F(EngineTest, UciNewGameClearsTTAndSearchHeuristics) {
    ASSERT_TRUE(execute("setoption name Threads value 2"));

    tt.advance_generation();
    tt.store(board().key(), Move(Square::E2, Square::E4), 42, 3, TTBound::Exact, 0);
    ASSERT_TRUE(tt.probe(board().key()).has_value());
    ASSERT_EQ(tt.current_generation(), std::uint8_t{1});

    for (size_t index = 0; index < thread_pool().thread_count(); ++index) {
        auto& ordering =
            SearchTestAccess::ordering(SearchThreadTestAccess::worker(thread_pool(), index));
        ordering.quiets.reward(WHITE, E2, E4, 4);
        ordering.continuations.reward(WHITE, PAWN, E4, KNIGHT, F6, 4);
    }

    EXPECT_TRUE(execute("ucinewgame"));

    EXPECT_FALSE(tt.probe(board().key()).has_value());
    EXPECT_EQ(tt.current_generation(), std::uint8_t{0});

    for (size_t index = 0; index < thread_pool().thread_count(); ++index) {
        auto& ordering =
            SearchTestAccess::ordering(SearchThreadTestAccess::worker(thread_pool(), index));
        EXPECT_EQ(ordering.quiets.get(WHITE, E2, E4), 0);
        EXPECT_EQ(ordering.continuations.get(WHITE, PAWN, E4, KNIGHT, F6), 0);
    }
}

TEST_F(EngineTest, ThreadOptionCommitsOnlyAfterSuccessfulResize) {
    EXPECT_TRUE(execute("setoption name tHrEaDs value 2"));

    EXPECT_EQ(thread_option_count(), 2);
    EXPECT_EQ(thread_pool().thread_count(), 2U);

    thread_pool().shutdown();
    EXPECT_TRUE(execute("setoption name Threads value 3"));

    EXPECT_EQ(thread_option_count(), 2);
    EXPECT_EQ(thread_pool().thread_count(), 2U);
    EXPECT_NE(output.str().find("error: failed to resize thread pool"), std::string::npos)
        << output.str();
}

TEST_F(EngineTest, PonderOptionValuesAreCaseInsensitive) {
    EXPECT_TRUE(execute("setoption name pOnDeR value ON"));
    EXPECT_TRUE(ponder_enabled());

    EXPECT_TRUE(execute("setoption name PONDER value oFf"));
    EXPECT_FALSE(ponder_enabled());
}

TEST_F(EngineTest, HashOptionResizesAndClearHashClearsTT) {
    ASSERT_TRUE(execute("setoption name Hash value 8"));
    ASSERT_EQ(hash_option_mb(), 8);
    ASSERT_EQ(tt.capacity_mb(), 8U);

    tt.store(board().key(), Move(Square::E2, Square::E4), 42, 3, TTBound::Exact, 0);
    ASSERT_TRUE(tt.probe(board().key()).has_value());

    EXPECT_TRUE(execute("setoption name Clear Hash"));

    EXPECT_FALSE(tt.probe(board().key()).has_value());
}

TEST_F(EngineTest, RegisterCommandIsSilentNoop) {
    EXPECT_TRUE(execute("register later"));

    EXPECT_TRUE(output.str().empty()) << output.str();
}

TEST_F(EngineTest, PonderSearchWaitsForHitAndPublishesExistingResult) {
    EXPECT_TRUE(execute("ponderhit"));
    EXPECT_TRUE(output.str().empty()) << output.str();

    EXPECT_TRUE(execute("position fen 7R/8/8/8/8/1K6/8/1k6 w - - 0 1"));
    EXPECT_TRUE(execute("go ponder mate 1 depth 10 nodes 0"));
    ASSERT_TRUE(wait_for_depth(1));

    EXPECT_TRUE(thread_pool().is_searching());
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 0);
    EXPECT_EQ(tt.current_generation(), std::uint8_t{1});

    EXPECT_TRUE(execute("ponderhit"));
    thread_pool().wait();

    const std::string transcript = output.str();
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1) << transcript;
    EXPECT_EQ(transcript.find("bestmove 0000"), std::string::npos) << transcript;
    EXPECT_EQ(tt.current_generation(), std::uint8_t{1});
}

TEST_F(EngineTest, MateLimitStopsAfterQualifyingCompletedDepth) {
    EXPECT_TRUE(execute("position fen 8/8/8/8/8/3K4/4Q3/k7 w - - 0 1"));
    EXPECT_TRUE(execute("go mate 2 depth 5"));
    thread_pool().wait();

    const RootLine snapshot = SearchThreadTestAccess::worker(thread_pool()).root_snapshot();
    EXPECT_EQ(snapshot.depth, 3);
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1);
    EXPECT_NE(output.str().find("score mate 2"), std::string::npos) << output.str();
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

TEST_F(EngineTest, IsReadyRespondsWhileSearchIsActive) {
    constexpr int ready_requests = 32;

    EXPECT_TRUE(execute("go"));
    ASSERT_TRUE(wait_for_busy());

    for (int i = 0; i < ready_requests; ++i)
        EXPECT_TRUE(execute("isready"));

    EXPECT_TRUE(execute("stop"));
    thread_pool().wait();

    const std::string  transcript     = output.str();
    int                ready_lines    = 0;
    int                bestmove_lines = 0;
    std::istringstream lines{transcript};
    for (std::string line; std::getline(lines, line);) {
        if (line == "readyok")
            ++ready_lines;
        else if (line.starts_with("bestmove "))
            ++bestmove_lines;
        else if (!line.starts_with("info "))
            ADD_FAILURE() << "interleaved output: " << line;
    }

    EXPECT_EQ(ready_lines, ready_requests) << transcript;
    EXPECT_EQ(bestmove_lines, 1) << transcript;
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

TEST_F(EngineTest, PositionReportsInvalidMoveToken) {
    EXPECT_TRUE(execute("position startpos moves e7e5"));

    EXPECT_EQ(board().to_fen(), board_test::fen::start);
    EXPECT_NE(output.str().find("error: invalid move in position command: e7e5"), std::string::npos)
        << output.str();
}

TEST_F(EngineTest, FailedPositionCommandsPreserveBoardAndHistory) {
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

TEST_F(EngineTest, MovesCommandFiltersIllegalPseudoLegalMoves) {
    EXPECT_TRUE(execute(std::format("position fen {}", board_test::fen::pinned_rook)));
    ASSERT_EQ(board().to_fen(), board_test::fen::pinned_rook);
    output.str("");
    output.clear();

    EXPECT_TRUE(execute("moves"));

    EXPECT_NE(output.str().find("e2e8"), std::string::npos) << output.str();
    EXPECT_EQ(output.str().find("e2a2"), std::string::npos) << output.str();
}

TEST_F(EngineTest, MoveCommandReportsInvalidMoveToken) {
    EXPECT_TRUE(execute("move notamove"));

    EXPECT_NE(output.str().find("invalid move: notamove"), std::string::npos) << output.str();
}

// setoption tests

struct SetOptionCase {
    std::string command;
    int         threads = uci::Options::default_threads;
    std::string output  = "error";
};

class SetOptionTest : public EngineTest, public ::testing::WithParamInterface<SetOptionCase> {};

TEST_P(SetOptionTest, ValidateSetOption) {
    const auto& param = GetParam();

    // Execute the setoption command
    EXPECT_TRUE(execute(param.command));

    // Check if the thread count is set correctly and output is as expected
    EXPECT_EQ(thread_pool().thread_count(), param.threads);
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
        SetOptionCase{.command = "setoption name Clear Hash value"},
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
    thread_pool().wait();
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
