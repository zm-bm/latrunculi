#include "uci_output.hpp"

#include <sstream>

#include "gtest/gtest.h"

using namespace std::string_literals;

class UCIOutputTest : public ::testing::Test {
   protected:
    std::ostringstream outputStream;
    UCIOutput uciOutput{outputStream};

    void SetUp() override {
        outputStream.str("");
        outputStream.clear();
    }
};

TEST_F(UCIOutputTest, Identify) {
    uciOutput.identify();
    EXPECT_NE(outputStream.str().find("uciok"), std::string::npos);
}

TEST_F(UCIOutputTest, Ready) {
    uciOutput.ready();
    EXPECT_EQ(outputStream.str(), "readyok\n");
}

TEST_F(UCIOutputTest, Bestmove) {
    std::string move = "e2e4";
    uciOutput.bestmove(move);
    EXPECT_EQ(outputStream.str(), "bestmove e2e4\n");
}

TEST_F(UCIOutputTest, InfoCentipawnScore) {
    int score = 50;
    int depth = 10;
    U64 nodes = 1000;
    Milliseconds ms(100);
    std::string pv = "e2e4 e7e5";
    UCIInfo uciInfo{score, depth, nodes, ms, pv};
    uciOutput.info(uciInfo);

    EXPECT_NE(outputStream.str().find("depth 10"), std::string::npos);
    EXPECT_NE(outputStream.str().find("score cp 50"), std::string::npos);
    EXPECT_NE(outputStream.str().find("nps 10"), std::string::npos);
    EXPECT_NE(outputStream.str().find("pv "s + pv), std::string::npos);
}

TEST_F(UCIOutputTest, InfoMateScore) {
    int score = MATE_SCORE - 4;
    int depth = 10;
    U64 nodes = 1000;
    Milliseconds ms(100);
    std::string pv = "e2e4 e7e5";
    UCIInfo uciInfo{score, depth, nodes, ms, pv};

    uciOutput.info(uciInfo);

    EXPECT_NE(outputStream.str().find("depth 10"), std::string::npos);
    EXPECT_NE(outputStream.str().find("score mate 2"), std::string::npos);
    EXPECT_NE(outputStream.str().find("nps 10"), std::string::npos);
    EXPECT_NE(outputStream.str().find("pv "s + pv), std::string::npos);
}

TEST_F(UCIOutputTest, InfoString) {
    std::string info = "This is a test info string";
    uciOutput.info(info);
    EXPECT_NE(outputStream.str().find(info), std::string::npos);
}

TEST_F(UCIOutputTest, Help) {
    uciOutput.help();
    EXPECT_NE(outputStream.str().find("Available commands"), std::string::npos);
}
