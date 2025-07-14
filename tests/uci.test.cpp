#include "uci.hpp"

#include <sstream>

#include "gtest/gtest.h"

using namespace std::string_literals;

// --------------------------
// UCIOption tests
// --------------------------

class UCIOptionTest : public ::testing::Test {
   protected:
    bool called = false;

    auto getCallback() {
        return [this](const auto&) { called = true; };
    }
};

TEST_F(UCIOptionTest, IntOptionBasic) {
    UCIOption<int> option(100, 0, 200, getCallback());
    EXPECT_EQ(option.value, 100);
    EXPECT_EQ(option.defaultValue, 100);
    EXPECT_EQ(*option.minValue, 0);
    EXPECT_EQ(*option.maxValue, 200);
}

TEST_F(UCIOptionTest, IntOptionSetValue) {
    UCIOption<int> option(100, 0, 200, getCallback());
    option.setValue("150");
    EXPECT_EQ(option.value, 150);
    EXPECT_TRUE(called);
}

TEST_F(UCIOptionTest, IntOptionSetSameValue) {
    UCIOption<int> option(100, 0, 200, getCallback());
    option.setValue("100");
    EXPECT_EQ(option.value, 100);
    EXPECT_FALSE(called);
}

TEST_F(UCIOptionTest, IntOptionMinBoundary) {
    UCIOption<int> option(100, 0, 200, getCallback());
    option.setValue("0");
    EXPECT_EQ(option.value, 0);
    EXPECT_THROW(option.setValue("-1"), std::out_of_range);
}

TEST_F(UCIOptionTest, IntOptionMaxBoundary) {
    UCIOption<int> option(100, 0, 200, getCallback());
    option.setValue("200");
    EXPECT_EQ(option.value, 200);
    EXPECT_THROW(option.setValue("201"), std::out_of_range);
}

TEST_F(UCIOptionTest, IntOptionInvalidFormat) {
    UCIOption<int> option(100, 0, 200);
    EXPECT_THROW(option.setValue("not a number"), std::invalid_argument);
}

TEST_F(UCIOptionTest, BoolOptionValues) {
    UCIOption<bool> option(false, std::nullopt, std::nullopt, getCallback());

    option.setValue("false");
    EXPECT_FALSE(called);

    option.setValue("true");
    EXPECT_TRUE(option.value);
    EXPECT_TRUE(called);
}

TEST_F(UCIOptionTest, BoolOptionInvalidValue) {
    UCIOption<bool> option(false);
    EXPECT_THROW(option.setValue("invalid"), std::invalid_argument);
}

TEST_F(UCIOptionTest, StringOption) {
    UCIOption<std::string> option("default", std::nullopt, std::nullopt, getCallback());

    option.setValue("default");
    EXPECT_FALSE(called);

    option.setValue("new value");
    EXPECT_EQ(option.value, "new value");
    EXPECT_TRUE(called);
}

// --------------------------
// UCIConfig tests
// --------------------------

class UCIConfigTest : public ::testing::Test {
   protected:
    UCIConfig config;
};

TEST_F(UCIConfigTest, RegisterGetIntOption) {
    config.registerOption<int>("Threads", 1, 1, 128);
    EXPECT_EQ(config.getOption<int>("Threads"), 1);
}

TEST_F(UCIConfigTest, RegisterGetBoolOption) {
    config.registerOption<bool>("Ponder", false);
    EXPECT_FALSE(config.getOption<bool>("Ponder"));
}

TEST_F(UCIConfigTest, RegisterGetStringOption) {
    config.registerOption<std::string>("BookPath", "book.bin");
    EXPECT_EQ(config.getOption<std::string>("BookPath"), "book.bin");
}

TEST_F(UCIConfigTest, SetIntOption) {
    config.registerOption<int>("Hash", 16, 1, 1024);
    config.setOption("Hash", "64");
    EXPECT_EQ(config.getOption<int>("Hash"), 64);
}

TEST_F(UCIConfigTest, SetBoolOption) {
    config.registerOption<bool>("UCI_Chess960", false);
    config.setOption("UCI_Chess960", "true");
    EXPECT_TRUE(config.getOption<bool>("UCI_Chess960"));
}

TEST_F(UCIConfigTest, SetStringOption) {
    config.registerOption<std::string>("SyzygyPath", "");
    config.setOption("SyzygyPath", "/path/to/syzygy");
    EXPECT_EQ(config.getOption<std::string>("SyzygyPath"), "/path/to/syzygy");
}

TEST_F(UCIConfigTest, OptionCallback) {
    bool called   = false;
    auto callback = [&](const int& val) { called = true; };
    config.registerOption<int>("MultiPV", 1, 1, 500, callback);
    config.setOption("MultiPV", "3");

    EXPECT_TRUE(called);
    EXPECT_EQ(config.getOption<int>("MultiPV"), 3);
}

TEST_F(UCIConfigTest, GetUnknownOption) {
    EXPECT_THROW(config.getOption<int>("NonExistentOption"), std::invalid_argument);
}

TEST_F(UCIConfigTest, TypeMismatch) {
    config.registerOption<int>("Depth", 10);
    EXPECT_THROW(config.getOption<bool>("Depth"), std::runtime_error);
}

TEST_F(UCIConfigTest, SetUnknownOption) {
    EXPECT_THROW(config.setOption("NonExistentOption", "value"), std::invalid_argument);
}

TEST_F(UCIConfigTest, SetOutOfRangeValue) {
    config.registerOption<int>("Selectivity", 2, 0, 4);
    EXPECT_THROW(config.setOption("Selectivity", "5"), std::out_of_range);
}

TEST_F(UCIConfigTest, OutputFormat) {
    config.registerOption<int>("Threads", 1, 1, 128);
    config.registerOption<bool>("Ponder", false);
    std::ostringstream oss;

    oss << config;

    std::cout << oss.str() << std::endl;
    EXPECT_NE(oss.str().find("option name Threads type spin default 1 min 1 max 128"),
              std::string::npos);
    EXPECT_NE(oss.str().find("option name Ponder type check default false"), std::string::npos);
}

// --------------------------
// UCIBestLine tests
// --------------------------

TEST(UCIBestLineTest, Output) {
    std::ostringstream oss;
    UCIBestLine bestLine{50, 10, 100, Milliseconds(1000), "e2e4 e7e5"};

    oss << bestLine;

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score cp 50"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("time 1000"), std::string::npos);
    EXPECT_NE(oss.str().find("pv e2e4 e7e5"), std::string::npos);
}

// --------------------------
// UCIProtocolHandler tests
// --------------------------

class UCIProtocolHandlerTest : public ::testing::Test {
   protected:
    std::ostringstream oss, err;
    UCIProtocolHandler uciHandler{oss, err};

    void SetUp() override {
        oss.str("");
        oss.clear();
    }
};

TEST_F(UCIProtocolHandlerTest, Help) {
    uciHandler.help();
    EXPECT_NE(err.str().find("Available commands"), std::string::npos);
}

TEST_F(UCIProtocolHandlerTest, Identify) {
    UCIConfig config;
    uciHandler.identify(config);
    EXPECT_NE(oss.str().find("uciok"), std::string::npos);
}

TEST_F(UCIProtocolHandlerTest, Ready) {
    uciHandler.ready();
    EXPECT_EQ(oss.str(), "readyok\n");
}

TEST_F(UCIProtocolHandlerTest, Bestmove) {
    std::string move = "e2e4";
    uciHandler.bestmove(move);
    EXPECT_EQ(oss.str(), "bestmove e2e4\n");
}

TEST_F(UCIProtocolHandlerTest, InfoBestLine) {
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

TEST_F(UCIProtocolHandlerTest, InfoScore) {
    int score = MATE_SCORE - 4;
    int depth = 10;
    U64 nodes = 1000;
    Milliseconds ms(100);
    std::string pv = "e2e4 e7e5";
    UCIBestLine bestLine{score, depth, nodes, ms, pv};

    uciHandler.info(bestLine);

    EXPECT_NE(oss.str().find("score mate 2"), std::string::npos);
    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv "s + pv), std::string::npos);
}

TEST_F(UCIProtocolHandlerTest, InfoString) {
    std::string info = "This is a test info string";
    uciHandler.info(info);
    EXPECT_NE(oss.str().find(info), std::string::npos);
}

TEST_F(UCIProtocolHandlerTest, LogOutput) {
    std::string logMessage = "This is a log message";
    uciHandler.logOutput(logMessage);
    EXPECT_EQ(err.str(), logMessage + "\n");
}
