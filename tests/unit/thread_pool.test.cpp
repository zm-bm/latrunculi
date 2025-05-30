#include "thread_pool.hpp"

#include <sstream>

#include "gtest/gtest.h"
#include "search_options.hpp"
#include "uci_output.hpp"

class ThreadPoolTest : public ::testing::Test {
   protected:
    std::ostringstream oss;
    UCIOutput uciOutput{oss};
    ThreadPool* threadPool;
    SearchOptions options;

    void SetUp() override {
        threadPool    = new ThreadPool(4, uciOutput);
        options.depth = 5;
    }

    void TearDown() override { delete threadPool; }
};

TEST_F(ThreadPoolTest, ConstructorInitializesThreads) { EXPECT_EQ(threadPool->getNodeCount(), 0); }

TEST_F(ThreadPoolTest, StartAllThreads) {
    threadPool->startAll(options);

    // Ensure threads are started
    EXPECT_NO_THROW(threadPool->waitAll());
}

TEST_F(ThreadPoolTest, StopAllThreads) {
    threadPool->startAll(options);
    threadPool->stopAll();

    // Ensure threads are stopped
    EXPECT_NO_THROW(threadPool->waitAll());
}

TEST_F(ThreadPoolTest, HaltAllThreads) {
    threadPool->startAll(options);
    threadPool->haltAll();

    // Ensure threads are halted
    EXPECT_NO_THROW(threadPool->waitAll());
}

TEST_F(ThreadPoolTest, GetNodeCount) {
    threadPool->startAll(options);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    threadPool->stopAll();

    int nodeCount = threadPool->getNodeCount();
    EXPECT_GT(nodeCount, 0);
}

TEST_F(ThreadPoolTest, GetStats) {
    SearchOptions options;
    threadPool->startAll(options);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    threadPool->stopAll();

    SearchStats stats = threadPool->getStats();
    if (STATS_ENABLED) {
        EXPECT_GT(stats.maxDepth(), 0);
    } else {
        SUCCEED();
    }
}
