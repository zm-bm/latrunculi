#include "thread.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <thread>

#include "board.hpp"
#include "search_options.hpp"
#include "test_util.hpp"
#include "thread_pool.hpp"
#include "uci.hpp"

class ThreadTest : public ::testing::Test {
protected:
    std::istringstream iss;
    std::ostringstream oss;
    uci::Protocol      protocol{oss, oss};
    ThreadPool         pool{1, protocol};
    Thread*            thread;

    void SetUp() override {
        thread = pool.threads[0].get();
        oss.str("");
    }
};

TEST_F(ThreadTest, ThreadShutsDownCorrectly) {
    Board         board{STARTFEN};
    SearchOptions options(iss, &board);

    // Start the search then shut down
    thread->start(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread->shutdown();

    // Check output for best move
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadTest, ThreadHaltsSearchCorrectly) {
    Board         board{STARTFEN};
    SearchOptions options(iss, &board);

    // Start the search then halt
    thread->start(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread->halt();

    // Check output for best move
    thread->wait();
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadTest, ThreadHandlesMultipleSearches) {
    Board         board1{STARTFEN};
    Board         board2{EMPTYFEN};
    SearchOptions options1(iss, &board1);
    SearchOptions options2(iss, &board2);

    // Run the first search
    thread->start(options1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread->halt();

    // Check output for the first search, clear the output stream
    thread->wait();
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
    oss.str("");

    // Run the second search
    thread->start(options2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread->halt();

    // Check output for the second search
    thread->wait();
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadTest, ThreadShutsDownGracefully) {
    thread->shutdown();
    SUCCEED();
}
