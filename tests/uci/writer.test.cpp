#include "uci/writer.hpp"

#include <barrier>
#include <sstream>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "search/root_line.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "uci/options.hpp"

namespace {

search::PrincipalVariation pv_for_move(Move move) {
    search::PrincipalVariation pv;
    search::PrincipalVariation child;
    pv.update(move, child);
    return pv;
}

search::PrincipalVariation pv_for_line(Move first, Move second) {
    search::PrincipalVariation child = pv_for_move(second);
    search::PrincipalVariation pv;
    pv.update(first, child);
    return pv;
}

} // namespace

class UciWriterTest : public ::testing::Test {
protected:
    std::ostringstream oss, err;
    uci::Writer        writer{oss, err};

    void SetUp() override {
        oss.str("");
        oss.clear();
        err.str("");
        err.clear();
    }

    std::string write_search_info(const search::RootLine& line,
                                  const Board&            board,
                                  NodeCount               nodes = 0,
                                  Milliseconds            time  = Milliseconds{0}) {
        writer.report_progress(line, board, nodes, time);
        std::string output = oss.str();
        oss.str("");
        oss.clear();
        return output;
    }
};

TEST_F(UciWriterTest, Help) {
    writer.help();
    EXPECT_NE(err.str().find("Available commands"), std::string::npos);
    EXPECT_NE(err.str().find("d / board"), std::string::npos);
}

TEST_F(UciWriterTest, Identify) {
    uci::Options               options;
    search::TranspositionTable default_table;

    writer.identify(options);

    EXPECT_EQ(default_table.capacity_mb(), static_cast<std::size_t>(options.hash.default_value));
    EXPECT_NE(oss.str().find("uciok"), std::string::npos);
    EXPECT_NE(oss.str().find("option name Hash type spin default 32 min 1 max 2048"),
              std::string::npos);
    EXPECT_NE(oss.str().find("option name Clear Hash type button"), std::string::npos);
    EXPECT_NE(oss.str().find("option name Ponder type check default false"), std::string::npos);
    EXPECT_EQ(oss.str().find("option name Debug"), std::string::npos);
}

TEST_F(UciWriterTest, Ready) {
    writer.ready();
    EXPECT_EQ(oss.str(), "readyok\n");
}

TEST_F(UciWriterTest, Bestmove) {
    Move move{E2, E4};
    writer.report_best_move(move);
    EXPECT_EQ(oss.str(), "bestmove e2e4\n");
}

TEST_F(UciWriterTest, BestmoveFormatsNullMoveAsUciNullMove) {
    writer.report_best_move(NULL_MOVE);
    EXPECT_EQ(oss.str(), "bestmove 0000\n");
}

TEST_F(UciWriterTest, SearchProgressWritesScoreFormats) {
    struct ScoreCase {
        int         value;
        std::string expected_score;
    };

    const ScoreCase cases[] = {
        {.value = 50, .expected_score = "score cp 50"},
        {.value = eval_value::mate - 4, .expected_score = "score mate 2"},
        {.value = -(eval_value::mate - 4), .expected_score = "score mate -2"},
    };

    Board board{board_test::fen::start};
    for (const ScoreCase& test_case : cases) {
        search::RootLine line{
            .root_move = Move{E2, E4},
            .value     = test_case.value,
            .depth     = 10,
            .completed = true,
            .pv        = pv_for_line(Move{E2, E4}, Move{E7, E5}),
        };

        const std::string output = write_search_info(line, board, 1000, Milliseconds{100});

        EXPECT_NE(output.find("depth 10"), std::string::npos);
        EXPECT_NE(output.find(test_case.expected_score), std::string::npos);
        EXPECT_NE(output.find("nps 10"), std::string::npos);
        EXPECT_NE(output.find("pv e2e4 e7e5"), std::string::npos);
    }
}

TEST_F(UciWriterTest, SearchProgressSerializesLegalRootPv) {
    Board            board{board_test::fen::start};
    search::RootLine line{
        .root_move = Move{E2, E4},
        .value     = 20,
        .depth     = 2,
        .completed = true,
        .pv        = pv_for_line(Move{E2, E4}, Move{E7, E5}),
    };

    EXPECT_EQ(write_search_info(line, board, 1234, Milliseconds{56}),
              "info depth 2 score cp 20 nodes 1234 time 56 nps 22035 pv e2e4 e7e5\n");
}

TEST_F(UciWriterTest, SearchProgressClearsUnusableRootPv) {
    Board board{board_test::fen::start};

    search::RootLine null_best{
        .root_move = NULL_MOVE,
        .value     = eval_value::draw,
        .depth     = 1,
        .completed = true,
    };
    EXPECT_EQ(write_search_info(null_best, board),
              "info depth 1 score cp 0 nodes 0 time 0 nps 0\n");

    search::RootLine incomplete{
        .root_move = Move{E2, E4},
        .value     = eval_value::draw,
        .depth     = 1,
        .completed = false,
        .pv        = pv_for_move(Move{E2, E4}),
    };
    EXPECT_EQ(write_search_info(incomplete, board),
              "info depth 1 score cp 0 nodes 0 time 0 nps 0\n");

    search::RootLine depth_zero{
        .root_move = Move{E2, E4},
        .value     = eval_value::draw,
        .depth     = 0,
        .completed = true,
        .pv        = pv_for_move(Move{E2, E4}),
    };
    EXPECT_EQ(write_search_info(depth_zero, board),
              "info depth 0 score cp 0 nodes 0 time 0 nps 0\n");
}

TEST_F(UciWriterTest, SearchProgressRejectsStaleRootPv) {
    Board board{board_test::fen::start};

    search::RootLine first_move_mismatch{
        .root_move = Move{E2, E4},
        .value     = eval_value::draw,
        .depth     = 1,
        .completed = true,
        .pv        = pv_for_move(Move{D2, D4}),
    };
    EXPECT_EQ(write_search_info(first_move_mismatch, board),
              "info depth 1 score cp 0 nodes 0 time 0 nps 0\n");

    search::RootLine illegal_child{
        .root_move = Move{E2, E4},
        .value     = eval_value::draw,
        .depth     = 2,
        .completed = true,
        .pv        = pv_for_line(Move{E2, E4}, Move{H1, H2}),
    };
    EXPECT_EQ(write_search_info(illegal_child, board),
              "info depth 2 score cp 0 nodes 0 time 0 nps 0\n");
}

TEST_F(UciWriterTest, InfoString) {
    std::string info = "This is a test info string";
    writer.info_string(info);
    EXPECT_NE(oss.str().find(info), std::string::npos);
}

TEST_F(UciWriterTest, InfoStringSanitizesLineBreaks) {
    writer.info_string("one\ntwo\rthree");

    EXPECT_EQ(oss.str(), "info string one two three\n");
}

TEST_F(UciWriterTest, DiagnosticLineAppendsNewline) {
    writer.diagnostic_line("This is a diagnostic");

    EXPECT_EQ(err.str(), "This is a diagnostic\n");
}

TEST_F(UciWriterTest, DiagnosticTextWritesRawText) {
    writer.diagnostic_text("one\ntwo\n");

    EXPECT_EQ(err.str(), "one\ntwo\n");
}

TEST(UciWriterConcurrencyTest, SerializesSharedOutputAndErrorStream) {
    constexpr int writes_per_thread = 128;

    std::ostringstream output;
    uci::Writer        writer{output, output};
    std::barrier       start_line{2};

    std::jthread output_thread([&] {
        start_line.arrive_and_wait();
        for (int i = 0; i < writes_per_thread; ++i) {
            writer.info_string("output");
            std::this_thread::yield();
        }
    });

    std::jthread error_thread([&] {
        start_line.arrive_and_wait();
        for (int i = 0; i < writes_per_thread; ++i) {
            writer.diagnostic_line("error");
            std::this_thread::yield();
        }
    });

    output_thread.join();
    error_thread.join();

    int                output_lines = 0;
    int                error_lines  = 0;
    std::istringstream lines{output.str()};
    for (std::string line; std::getline(lines, line);) {
        if (line == "info string output")
            ++output_lines;
        else if (line == "error")
            ++error_lines;
        else
            ADD_FAILURE() << "interleaved output: " << line;
    }

    EXPECT_EQ(output_lines, writes_per_thread);
    EXPECT_EQ(error_lines, writes_per_thread);
}
