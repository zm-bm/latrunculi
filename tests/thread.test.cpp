#include "thread.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <thread>

#include "board.hpp"
#include "search_options.hpp"
#include "thread_pool.hpp"
#include "uci.hpp"

class ThreadTest : public ::testing::Test {
   protected:
    std::ostringstream oss;
    UCIProtocolHandler uciHandler{oss, oss};
    ThreadPool threadPool{1, uciHandler};
    Thread* thread;

    void SetUp() override { thread = threadPool.threads[0].get(); }
};

TEST_F(ThreadTest, ThreadStartsAndExitsCorrectly) {
    SearchOptions options;
    Board board{STARTFEN};
    options.board = &board;
    thread->set(options, Clock::now());

    thread->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    thread->exit();
    thread->wait();

    // If the thread stops and waits without deadlock, the test passes.
    SUCCEED();
}

TEST_F(ThreadTest, ThreadProcessesSearchCorrectly) {
    SearchOptions options;
    Board board{STARTFEN};
    options.board = &board;

    thread->set(options, Clock::now());

    thread->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    thread->exit();
    thread->wait();

    EXPECT_NE(oss.str().find("bestmove"), std::string::npos)
        << "Expected 'bestmove' in output, but got: " << oss.str();
}

TEST_F(ThreadTest, ThreadStopsSearchCorrectly) {
    SearchOptions options;
    Board board{STARTFEN};
    options.board = &board;

    thread->set(options, Clock::now());

    thread->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thread->stop();
    thread->wait();

    EXPECT_NE(oss.str().find("bestmove"), std::string::npos)
        << "Expected 'bestmove' in output, but got: " << oss.str();
}

TEST_F(ThreadTest, ThreadHandlesMultipleSearches) {
    Board board{EMPTYFEN};
    SearchOptions options1;
    options1.board = &board;

    thread->set(options1, Clock::now());
    thread->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    thread->stop();
    thread->wait();

    SearchOptions options2;
    options2.board = &board;

    thread->set(options2, Clock::now());
    thread->start();
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
