#include "uci/uci_writer.hpp"

#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "search/root_line.hpp"
#include "support/test_util.hpp"

namespace {

PrincipalVariation pv_for_move(Move move) {
    PrincipalVariation pv;
    PrincipalVariation child;
    pv.update(move, child);
    return pv;
}

PrincipalVariation pv_for_line(Move first, Move second) {
    PrincipalVariation child = pv_for_move(second);
    PrincipalVariation pv;
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

    std::string write_search_info(const RootLine& line,
                                  const Board&    board,
                                  uint64_t        nodes = 0,
                                  Milliseconds    time  = Milliseconds{0}) {
        writer.search_info(line, board, nodes, time);
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
    EXPECT_NE(err.str().find("reference/uci-protocol-specification.txt"), std::string::npos);
}

TEST_F(UciWriterTest, Identify) {
    uci::Options options;
    writer.identify(options);
    EXPECT_NE(oss.str().find("uciok"), std::string::npos);
    EXPECT_NE(oss.str().find("option name Clear Hash type button"), std::string::npos);
    EXPECT_NE(oss.str().find("option name Debug type check default false"), std::string::npos);
}

TEST_F(UciWriterTest, Ready) {
    writer.ready();
    EXPECT_EQ(oss.str(), "readyok\n");
}

TEST_F(UciWriterTest, Bestmove) {
    Move move{E2, E4};
    writer.bestmove(move);
    EXPECT_EQ(oss.str(), "bestmove e2e4\n");
}

TEST_F(UciWriterTest, BestmoveFormatsNullMoveAsUciNullMove) {
    writer.bestmove(NULL_MOVE);
    EXPECT_EQ(oss.str(), "bestmove 0000\n");
}

TEST_F(UciWriterTest, FormatBestmoveFormatsNullMoveAsUciNullMove) {
    EXPECT_EQ(uci::format_bestmove(NULL_MOVE), "bestmove 0000");
}

TEST_F(UciWriterTest, SearchProgressWritesScoreFormats) {
    struct ScoreCase {
        int         value;
        std::string expected_score;
    };

    const ScoreCase cases[] = {
        {.value = 50, .expected_score = "score cp 50"},
        {.value = MATE_VALUE - 4, .expected_score = "score mate 2"},
        {.value = -(MATE_VALUE - 4), .expected_score = "score mate -2"},
    };

    TestBoard board{STARTFEN};
    for (const ScoreCase& test_case : cases) {
        RootLine line{
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
    TestBoard board{STARTFEN};
    RootLine  line{
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
    TestBoard board{STARTFEN};

    RootLine null_best{
        .root_move = NULL_MOVE,
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = true,
    };
    EXPECT_EQ(write_search_info(null_best, board),
              "info depth 1 score cp 0 nodes 0 time 0 nps 0\n");

    RootLine incomplete{
        .root_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = false,
        .pv        = pv_for_move(Move{E2, E4}),
    };
    EXPECT_EQ(write_search_info(incomplete, board),
              "info depth 1 score cp 0 nodes 0 time 0 nps 0\n");

    RootLine depth_zero{
        .root_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 0,
        .completed = true,
        .pv        = pv_for_move(Move{E2, E4}),
    };
    EXPECT_EQ(write_search_info(depth_zero, board),
              "info depth 0 score cp 0 nodes 0 time 0 nps 0\n");
}

TEST_F(UciWriterTest, SearchProgressRejectsStaleRootPv) {
    TestBoard board{STARTFEN};

    RootLine first_move_mismatch{
        .root_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = true,
        .pv        = pv_for_move(Move{D2, D4}),
    };
    EXPECT_EQ(write_search_info(first_move_mismatch, board),
              "info depth 1 score cp 0 nodes 0 time 0 nps 0\n");

    RootLine illegal_child{
        .root_move = Move{E2, E4},
        .value     = DRAW_VALUE,
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

TEST_F(UciWriterTest, DebugOutput) {
    std::string logMessage = "This is a log message";
    writer.debug(logMessage);
    EXPECT_EQ(err.str(), logMessage + "\n");
}

TEST_F(UciWriterTest, DebugTextWritesRawText) {
    writer.debug_text("one\ntwo\n");

    EXPECT_EQ(err.str(), "one\ntwo\n");
}
