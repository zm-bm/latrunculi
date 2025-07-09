#include "thread_pool.hpp"

#include <sstream>

#include "gtest/gtest.h"
#include "search_options.hpp"
#include "thread.hpp"
#include "uci.hpp"

const int N_THREADS = 4;

class ThreadPoolTest : public ::testing::Test {
   protected:
    std::ostringstream oss;
    UCIProtocolHandler uciHandler{oss, oss};
    ThreadPool* threadPool;
    SearchOptions options;

    U64 testAccumulate() { return threadPool->accumulate(&Thread::nodes); }

    void SetUp() override {
        threadPool    = new ThreadPool(N_THREADS, uciHandler);
        options.depth = 5;
    }

    void TearDown() override { delete threadPool; }
};

TEST_F(ThreadPoolTest, Constructor) { EXPECT_EQ(threadPool->size(), N_THREADS); }

TEST_F(ThreadPoolTest, StartAllThreads) {
    threadPool->startAll(options);

    // Ensure threads are started
    EXPECT_NO_THROW(threadPool->waitAll());
}

TEST_F(ThreadPoolTest, ExitAllThreads) {
    threadPool->startAll(options);
    threadPool->exitAll();

    // Ensure threads are stopped
    EXPECT_NO_THROW(threadPool->waitAll());
}

TEST_F(ThreadPoolTest, StopAllThreads) {
    threadPool->startAll(options);
    threadPool->stopAll();

    // Ensure threads are halted
    EXPECT_NO_THROW(threadPool->waitAll());
}

TEST_F(ThreadPoolTest, AccumulateNodes) {
    threadPool->startAll(options);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    threadPool->exitAll();

    U64 totalNodes = testAccumulate();

    EXPECT_GT(totalNodes, 0);
}
