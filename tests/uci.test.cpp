#include "uci.hpp"

#include <sstream>

#include "gtest/gtest.h"

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
TEST_F(ProtocolTest, InfoPV) {
    uci::PV pv{
        .score = 50,
        .depth = 10,
        .nodes = 1000,
        .time  = Milliseconds(100),
        .moves = "e2e4 e7e5",
    };

    protocol.info(pv);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score cp 50"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv e2e4 e7e5"), std::string::npos);
}

TEST_F(ProtocolTest, InfoMate) {
    uci::PV pv{
        .score = MATE_VALUE - 4,
        .depth = 10,
        .nodes = 1000,
        .time  = Milliseconds(100),
        .moves = "e2e4 e7e5",
    };

    protocol.info(pv);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score mate 2"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv e2e4 e7e5"), std::string::npos);
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
