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
    std::istringstream iss;
    std::ostringstream oss;
    uci::Protocol      protocol{oss, oss};
    ThreadPool         pool{N_THREADS, protocol};
    Board              board{STARTFEN};
    SearchOptions      options{iss, &board};

    uint64_t accumulate_nodes() { return pool.accumulate(&Thread::nodes); }

    void SetUp() override { oss.str(""); }
};

TEST_F(ThreadPoolTest, Constructor) {
    EXPECT_EQ(pool.size(), N_THREADS);
}

TEST_F(ThreadPoolTest, StartAllThreads) {
    // Start the pool with fixed depth
    options.depth = 5;
    pool.start_all(options);

    // Check output for best move
    EXPECT_NO_THROW(pool.wait_all());
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadPoolTest, HaltAllThreads) {
    // Start the pool and then halt
    pool.start_all(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.halt_all();

    // Check output for best move
    EXPECT_NO_THROW(pool.wait_all());
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadPoolTest, ShutdownAllThreads) {
    // Start the pool and then shutdown all threads
    pool.start_all(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.shutdown_all();

    // Check output for best move
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadPoolTest, AccumulateNodes) {
    // Start the pool and then exit
    pool.start_all(options);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.shutdown_all();

    // Check that nodes were accumulated
    EXPECT_GT(accumulate_nodes(), 0);
}
