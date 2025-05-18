#include "thread.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <thread>

#include "search_options.hpp"
#include "thread_pool.hpp"
#include "uci_output.hpp"

class ThreadTest : public ::testing::Test {
   protected:
    std::ostringstream oss;
    UCIOutput uciOutput{oss};
    Thread* thread;

    void SetUp() override { thread = new Thread(0, uciOutput, nullptr); }

    void TearDown() override { delete thread; }
};

TEST_F(ThreadTest, ThreadStartsAndStopsCorrectly) {
    thread->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    thread->stop();
    thread->wait();

    // If the thread stops and waits without deadlock, the test passes.
    SUCCEED();
}

TEST_F(ThreadTest, ThreadProcessesSearchCorrectly) {
    SearchOptions options;
    options.fen   = STARTFEN;
    options.debug = false;

    thread->set(options, Clock::now());

    thread->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    thread->stop();
    thread->wait();

    EXPECT_NE(oss.str().find("bestmove"), std::string::npos)
        << "Expected 'bestmove' in output, but got: " << oss.str();
}

TEST_F(ThreadTest, ThreadExitsGracefully) {
    thread->stop();
    thread->wait();

    // If the thread exits without issues, the test passes.
    SUCCEED();
}
