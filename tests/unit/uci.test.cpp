#include "uci.hpp"

#include <sstream>

#include "gtest/gtest.h"

using namespace std::string_literals;

class UCIOutputTest : public ::testing::Test {
   protected:
    std::ostringstream oss;
    UCIProtocolHandler uciHandler{oss, oss};

    void SetUp() override {
        oss.str("");
        oss.clear();
    }
};

TEST_F(UCIOutputTest, Identify) {
    uciHandler.identify();
    EXPECT_NE(oss.str().find("uciok"), std::string::npos);
}

TEST_F(UCIOutputTest, Ready) {
    uciHandler.ready();
    EXPECT_EQ(oss.str(), "readyok\n");
}

TEST_F(UCIOutputTest, Bestmove) {
    std::string move = "e2e4";
    uciHandler.bestmove(move);
    EXPECT_EQ(oss.str(), "bestmove e2e4\n");
}

TEST_F(UCIOutputTest, InfoCentipawnScore) {
    int score = 50;
    int depth = 10;
    U64 nodes = 1000;
    Milliseconds ms(100);
    std::string pv = "e2e4 e7e5";
    UCIBestLine bestLine{score, depth, nodes, ms, pv};
    uciHandler.info(bestLine);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score cp 50"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv "s + pv), std::string::npos);
}

TEST_F(UCIOutputTest, InfoMateScore) {
    int score = MATE_SCORE - 4;
    int depth = 10;
    U64 nodes = 1000;
    Milliseconds ms(100);
    std::string pv = "e2e4 e7e5";
    UCIBestLine bestLine{score, depth, nodes, ms, pv};

    uciHandler.info(bestLine);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score mate 2"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv "s + pv), std::string::npos);
}

TEST_F(UCIOutputTest, InfoString) {
    std::string info = "This is a test info string";
    uciHandler.info(info);
    EXPECT_NE(oss.str().find(info), std::string::npos);
}

TEST_F(UCIOutputTest, Help) {
    uciHandler.help();
    EXPECT_NE(oss.str().find("Available commands"), std::string::npos);
}
