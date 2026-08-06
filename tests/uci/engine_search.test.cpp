#include "support/engine_test_fixture.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "board/board.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"
#include "gtest/gtest.h"

class EngineSearchTest : public EngineTest {
protected:
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
        search::Worker& worker   = SearchThreadTestAccess::worker(thread_pool());
        const auto      deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (worker.root_snapshot().depth >= depth)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return worker.root_snapshot().depth >= depth;
    }
};

TEST_F(EngineSearchTest, ImmediateStopReportsLegalMove) {
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

TEST_F(EngineSearchTest, InfiniteSearchWaitsForStop) {
    EXPECT_TRUE(execute("go infinite depth 1 nodes 0"));

    ASSERT_TRUE(wait_for_depth(1));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(thread_pool().is_searching());
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 0);

    EXPECT_TRUE(execute("stop"));
    thread_pool().wait();

    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1) << output.str();
}

TEST_F(EngineSearchTest, GoDepthReportsFreshFinalInformationBeforeBestmove) {
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

TEST_F(EngineSearchTest, GoPreservesWideNodeLimit) {
    constexpr NodeCount node_limit = static_cast<NodeCount>(std::numeric_limits<int>::max()) + 1;

    EXPECT_TRUE(execute(std::format("go depth 1 nodes {}", node_limit)));
    thread_pool().wait();

    const auto& limits = SearchTestAccess::limits(SearchThreadTestAccess::worker(thread_pool()));
    EXPECT_EQ(limits.nodes, node_limit);
}

TEST_F(EngineSearchTest, SearchmovesRestrictAndValidateRootMoves) {
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

TEST_F(EngineSearchTest, GoWhileSearchInProgressIsRejected) {
    EXPECT_TRUE(execute("go"));
    ASSERT_TRUE(wait_for_busy());

    EXPECT_TRUE(execute("go searchmoves invalid"));
    EXPECT_TRUE(execute("stop"));
    thread_pool().wait();

    EXPECT_NE(output.str().find("search already in progress"), std::string::npos) << output.str();
    EXPECT_EQ(output.str().find("invalid searchmove"), std::string::npos) << output.str();
}

TEST_F(EngineSearchTest, StateChangingCommandsWhileSearchInProgressAreRejected) {
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

TEST_F(EngineSearchTest, TerminalPositionsReportDepthZeroAndNullBestMove) {
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

TEST_F(EngineSearchTest, UciNewGameClearsTTAndSearchHeuristics) {
    ASSERT_TRUE(execute("setoption name Threads value 2"));

    search::tt.advance_generation();
    search::tt.store(board().key(), Move(Square::E2, Square::E4), 42, 3, search::TTBound::Exact, 0);
    ASSERT_TRUE(search::tt.probe(board().key()).has_value());
    ASSERT_EQ(search::tt.current_generation(), std::uint8_t{1});

    for (size_t index = 0; index < thread_pool().thread_count(); ++index) {
        auto& ordering_state =
            SearchTestAccess::ordering_state(SearchThreadTestAccess::worker(thread_pool(), index));
        ordering_state.quiets.reward(WHITE, E2, E4, 4);
        ordering_state.continuations.reward(WHITE, PAWN, E4, KNIGHT, F6, 4);
    }

    EXPECT_TRUE(execute("ucinewgame"));

    EXPECT_FALSE(search::tt.probe(board().key()).has_value());
    EXPECT_EQ(search::tt.current_generation(), std::uint8_t{0});

    for (size_t index = 0; index < thread_pool().thread_count(); ++index) {
        auto& ordering_state =
            SearchTestAccess::ordering_state(SearchThreadTestAccess::worker(thread_pool(), index));
        EXPECT_EQ(ordering_state.quiets.get(WHITE, E2, E4), 0);
        EXPECT_EQ(ordering_state.continuations.get(WHITE, PAWN, E4, KNIGHT, F6), 0);
    }
}

TEST_F(EngineSearchTest, PonderSearchWaitsForHitAndPublishesExistingResult) {
    EXPECT_TRUE(execute("ponderhit"));
    EXPECT_TRUE(output.str().empty()) << output.str();

    EXPECT_TRUE(execute("position fen 7R/8/8/8/8/1K6/8/1k6 w - - 0 1"));
    EXPECT_TRUE(execute("go ponder mate 1 depth 10 nodes 0"));
    ASSERT_TRUE(wait_for_depth(1));

    EXPECT_TRUE(thread_pool().is_searching());
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 0);
    EXPECT_EQ(search::tt.current_generation(), std::uint8_t{1});

    EXPECT_TRUE(execute("ponderhit"));
    thread_pool().wait();

    const std::string transcript = output.str();
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1) << transcript;
    EXPECT_EQ(transcript.find("bestmove 0000"), std::string::npos) << transcript;
    EXPECT_EQ(search::tt.current_generation(), std::uint8_t{1});
}

TEST_F(EngineSearchTest, MateLimitStopsAfterQualifyingCompletedDepth) {
    EXPECT_TRUE(execute("position fen 8/8/8/8/8/3K4/4Q3/k7 w - - 0 1"));
    EXPECT_TRUE(execute("go mate 2 depth 5"));
    thread_pool().wait();

    const search::RootLine snapshot = SearchThreadTestAccess::worker(thread_pool()).root_snapshot();
    EXPECT_EQ(snapshot.depth, 3);
    EXPECT_EQ(count_output_lines_starting_with("bestmove "), 1);
    EXPECT_NE(output.str().find("score mate 2"), std::string::npos) << output.str();
}

TEST_F(EngineSearchTest, IsReadyRespondsWhileSearchIsActive) {
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

struct GoCase {
    std::string command;
    std::string output;
};

class GoTest : public EngineSearchTest, public ::testing::WithParamInterface<GoCase> {};

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
