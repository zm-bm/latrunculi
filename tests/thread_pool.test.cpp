#include "thread_pool.hpp"

#include <sstream>

#include "search_options.hpp"
#include "test_util.hpp"
#include "thread.hpp"
#include "uci.hpp"
#include "gtest/gtest.h"

const int N_THREADS = 4;

class ThreadPoolTest : public ::testing::Test {
protected:
    std::ostringstream oss;
    uci::Protocol      protocol{oss, oss};
    ThreadPool*        pool;
    Board              board{STARTFEN};
    SearchOptions      options;

    uint64_t test_accumulate() { return pool->accumulate(&Thread::nodes); }

    void SetUp() override {
        pool          = new ThreadPool(N_THREADS, protocol);
        options.board = &board;
        options.depth = 5;
    }

    void TearDown() override { delete pool; }
};

TEST_F(ThreadPoolTest, Constructor) {
    EXPECT_EQ(pool->size(), N_THREADS);
}

TEST_F(ThreadPoolTest, StartAllThreads) {
    pool->start_all(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Ensure threads are started
    EXPECT_NO_THROW(pool->wait_all());
}

TEST_F(ThreadPoolTest, ExitAllThreads) {
    pool->start_all(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool->exit_all();

    // Ensure threads are stopped
    EXPECT_NO_THROW(pool->wait_all());
}

TEST_F(ThreadPoolTest, StopAllThreads) {
    pool->start_all(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool->stop_all();

    // Ensure threads are halted
    EXPECT_NO_THROW(pool->wait_all());
}

TEST_F(ThreadPoolTest, AccumulateNodes) {
    pool->start_all(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool->exit_all();

    uint64_t totalNodes = test_accumulate();

    EXPECT_GT(totalNodes, 0);
}
