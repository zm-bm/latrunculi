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
    std::ostringstream oss;
    uci::Protocol      protocol{oss, oss};
    ThreadPool         pool{1, protocol};
    Thread*            thread;

    void SetUp() override { thread = pool.threads[0].get(); }
};

TEST_F(ThreadTest, ThreadStartsAndExitsCorrectly) {
    Board         board{STARTFEN};
    SearchOptions options;
    options.board = &board;

    thread->start(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    thread->exit();
    thread->wait();

    // If the thread stops and waits without deadlock, the test passes.
    SUCCEED();
}

TEST_F(ThreadTest, ThreadProcessesSearchCorrectly) {
    SearchOptions options;
    Board         board{STARTFEN};
    options.board = &board;

    thread->start(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    thread->exit();
    thread->wait();

    EXPECT_NE(oss.str().find("bestmove"), std::string::npos)
        << "Expected 'bestmove' in output, but got: " << oss.str();
}

TEST_F(ThreadTest, ThreadStopsSearchCorrectly) {
    SearchOptions options;
    Board         board{STARTFEN};
    options.board = &board;

    thread->start(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thread->stop();
    thread->wait();

    EXPECT_NE(oss.str().find("bestmove"), std::string::npos)
        << "Expected 'bestmove' in output, but got: " << oss.str();
}

TEST_F(ThreadTest, ThreadHandlesMultipleSearches) {
    Board         board{EMPTYFEN};
    SearchOptions options1;
    options1.board = &board;

    thread->start(options1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thread->stop();
    thread->wait();

    SearchOptions options2;
    options2.board = &board;

    thread->start(options2);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thread->stop();
    thread->wait();

    EXPECT_NE(oss.str().find("bestmove"), std::string::npos)
        << "Expected 'bestmove' in output, but got: " << oss.str();
}

TEST_F(ThreadTest, ThreadExitsGracefully) {
    thread->exit();
    thread->wait();

    // If the thread exits without issues, the test passes.
    SUCCEED();
}
