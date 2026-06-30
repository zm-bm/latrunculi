#include "uci.hpp"

#include <sstream>
#include <stdexcept>

#include "root_line.hpp"
#include "test_util.hpp"

#include "gtest/gtest.h"

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

class ProtocolTest : public ::testing::Test {
protected:
    std::ostringstream oss, err;
    uci::Protocol      protocol{oss, err};

    void SetUp() override {
        oss.str("");
        oss.clear();
        err.str("");
        err.clear();
    }
};

TEST_F(ProtocolTest, Help) {
    protocol.help();
    EXPECT_NE(err.str().find("Available commands"), std::string::npos);
}

TEST_F(ProtocolTest, Identify) {
    uci::Config config;
    protocol.identify(config);
    EXPECT_NE(oss.str().find("uciok"), std::string::npos);
    EXPECT_NE(oss.str().find("option name Debug type check default false"), std::string::npos);
}

TEST_F(ProtocolTest, ConfigSetOptionWorksWithoutCallbacks) {
    uci::Config config;

    EXPECT_NO_THROW(config.set_option("Hash", "16"));
    EXPECT_NO_THROW(config.set_option("Threads", "2"));
    EXPECT_NO_THROW(config.set_option("Debug", "on"));

    EXPECT_EQ(config.hash.value, 16);
    EXPECT_EQ(config.threads.value, 2);
    EXPECT_TRUE(config.debug.value);
}

TEST_F(ProtocolTest, ConfigRejectsMalformedOptionValues) {
    uci::Config config;

    EXPECT_THROW(config.set_option("Threads", "2 extra"), std::invalid_argument);
    EXPECT_THROW(config.set_option("Debug", "maybe"), std::invalid_argument);
}

TEST_F(ProtocolTest, Ready) {
    protocol.ready();
    EXPECT_EQ(oss.str(), "readyok\n");
}

TEST_F(ProtocolTest, Bestmove) {
    std::string move = "e2e4";
    protocol.bestmove(move);
    EXPECT_EQ(oss.str(), "bestmove e2e4\n");
}

TEST_F(ProtocolTest, InfoSearchInfo) {
    uci::SearchInfo info{
        .score = 50,
        .depth = 10,
        .nodes = 1000,
        .time  = Milliseconds{100},
        .pv    = "e2e4 e7e5",
    };

    protocol.info(info);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score cp 50"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv e2e4 e7e5"), std::string::npos);
}

TEST_F(ProtocolTest, InfoMate) {
    uci::SearchInfo info{
        .score = MATE_VALUE - 4,
        .depth = 10,
        .nodes = 1000,
        .time  = Milliseconds{100},
        .pv    = "e2e4 e7e5",
    };

    protocol.info(info);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score mate 2"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv e2e4 e7e5"), std::string::npos);
}

TEST_F(ProtocolTest, MakeSearchInfoSerializesLegalRootPv) {
    TestBoard board{STARTFEN};
    RootLine  line{
         .best_move = Move{E2, E4},
         .value     = 20,
         .depth     = 2,
         .completed = true,
         .pv        = pv_for_line(Move{E2, E4}, Move{E7, E5}),
    };

    const auto info = uci::make_search_info(line, board, 1234, Milliseconds{56});

    EXPECT_EQ(info.score, line.value);
    EXPECT_EQ(info.depth, line.depth);
    EXPECT_EQ(info.nodes, 1234U);
    EXPECT_EQ(info.time, Milliseconds{56});
    EXPECT_EQ(info.pv, "e2e4 e7e5 ");
}

TEST_F(ProtocolTest, MakeSearchInfoClearsUnusableRootPv) {
    TestBoard board{STARTFEN};

    RootLine null_best{
        .best_move = NULL_MOVE,
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = true,
    };
    EXPECT_EQ(uci::make_search_info(null_best, board, 0, Milliseconds{0}).pv, "");

    RootLine incomplete{
        .best_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = false,
        .pv        = pv_for_move(Move{E2, E4}),
    };
    EXPECT_EQ(uci::make_search_info(incomplete, board, 0, Milliseconds{0}).pv, "");

    RootLine depth_zero{
        .best_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 0,
        .completed = true,
        .pv        = pv_for_move(Move{E2, E4}),
    };
    EXPECT_EQ(uci::make_search_info(depth_zero, board, 0, Milliseconds{0}).pv, "");
}

TEST_F(ProtocolTest, MakeSearchInfoRejectsStaleRootPv) {
    TestBoard board{STARTFEN};

    RootLine first_move_mismatch{
        .best_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = true,
        .pv        = pv_for_move(Move{D2, D4}),
    };
    EXPECT_EQ(uci::make_search_info(first_move_mismatch, board, 0, Milliseconds{0}).pv, "");

    RootLine illegal_child{
        .best_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 2,
        .completed = true,
        .pv        = pv_for_line(Move{E2, E4}, Move{H1, H2}),
    };
    EXPECT_EQ(uci::make_search_info(illegal_child, board, 0, Milliseconds{0}).pv, "");
}

TEST_F(ProtocolTest, InfoString) {
    std::string info = "This is a test info string";
    protocol.info(info);
    EXPECT_NE(oss.str().find(info), std::string::npos);
}

TEST_F(ProtocolTest, LogOutput) {
    std::string logMessage = "This is a log message";
    protocol.diagnostic_output(logMessage);
    EXPECT_EQ(err.str(), logMessage + "\n");
}
