#include "threading.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <thread>

#include "search_options.hpp"
#include "test_util.hpp"
#include "thread_test_access.hpp"
#include "tt.hpp"
#include "uci.hpp"

namespace {

constexpr int THREAD_COUNT = 4;

class ThreadLifecycleTest : public ::testing::Test {
protected:
    std::istringstream iss;
    std::ostringstream oss;
    uci::Protocol      protocol{oss, oss};
    ThreadPool         pool{1, protocol};

    void SetUp() override {
        oss.str("");
        oss.clear();
    }

    Thread& test_thread() { return ThreadTestAccess::thread(pool); }

    bool
    wait_for_worker_running(std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (ThreadTestAccess::worker_running(test_thread()))
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return ThreadTestAccess::worker_running(test_thread());
    }

    bool has_bestmove_output() const { return oss.str().find("bestmove") != std::string::npos; }
};

class ThreadPoolTest : public ::testing::Test {
protected:
    std::istringstream iss;
    std::ostringstream oss;
    uci::Protocol      protocol{oss, oss};
    ThreadPool         pool{THREAD_COUNT, protocol};
    TestBoard          board{STARTFEN};
    SearchOptions      options{iss, &board};

    uint64_t nodes_searched() const { return pool.nodes_searched(); }

    void expect_completed_depths(int depth) {
        bool saw_completed_depth = false;
        for (size_t index = 0; index < pool.thread_count(); ++index) {
            auto snapshot = ThreadTestAccess::root_snapshot(pool, index);
            if (!snapshot.completed) {
                EXPECT_FALSE(snapshot.completed_depth());
                continue;
            }

            EXPECT_TRUE(snapshot.completed_depth());
            EXPECT_EQ(snapshot.depth, depth);
            saw_completed_depth = true;
        }
        EXPECT_TRUE(saw_completed_depth);
    }

    bool worker_running() { return ThreadTestAccess::worker_running(pool); }

    bool
    wait_for_worker_running(std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (worker_running())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return worker_running();
    }

    void SetUp() override {
        oss.str("");
        oss.clear();
        tt.clear();
    }

    bool has_bestmove_output() const { return oss.str().find("bestmove") != std::string::npos; }
};

} // namespace

TEST_F(ThreadLifecycleTest, ThreadShutsDownCorrectly) {
    TestBoard     board{STARTFEN};
    SearchOptions options(iss, &board);

    ThreadTestAccess::start_search(test_thread(), options);
    ASSERT_TRUE(wait_for_worker_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ThreadTestAccess::shutdown(test_thread());

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadLifecycleTest, ThreadStopsSearchCorrectly) {
    TestBoard     board{STARTFEN};
    SearchOptions options(iss, &board);

    ThreadTestAccess::start_search(test_thread(), options);
    ASSERT_TRUE(wait_for_worker_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ThreadTestAccess::request_stop(test_thread());

    ThreadTestAccess::wait_for_idle(test_thread());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadLifecycleTest, ThreadHandlesMultipleSearches) {
    TestBoard     board1{STARTFEN};
    TestBoard     board2{EMPTYFEN};
    SearchOptions options1(iss, &board1);
    SearchOptions options2(iss, &board2);

    ThreadTestAccess::start_search(test_thread(), options1);
    ThreadTestAccess::request_stop(test_thread());

    ThreadTestAccess::wait_for_idle(test_thread());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
    oss.str("");
    oss.clear();

    ThreadTestAccess::start_search(test_thread(), options2);
    ThreadTestAccess::request_stop(test_thread());

    ThreadTestAccess::wait_for_idle(test_thread());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadLifecycleTest, ThreadObjectSizeStaysBounded) {
    EXPECT_LT(sizeof(Thread), 65536U);
}

TEST_F(ThreadLifecycleTest, ThreadShutsDownGracefully) {
    ThreadTestAccess::shutdown(test_thread());
    SUCCEED();
}

TEST_F(ThreadPoolTest, Constructor) {
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);
}

TEST_F(ThreadPoolTest, StartSearchRejectsEmptyPool) {
    ThreadPool empty_pool{0, protocol};

    EXPECT_FALSE(empty_pool.start_search(options));
}

TEST_F(ThreadPoolTest, StartSearchRejectsAfterShutdown) {
    pool.shutdown();

    EXPECT_FALSE(pool.start_search(options));
}

TEST_F(ThreadPoolTest, StartSearchCompletes) {
    options.depth = 5;
    EXPECT_TRUE(pool.start_search(options));

    EXPECT_NO_THROW(pool.wait());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, IsSearchingTracksLifecycle) {
    options.depth = 5;

    EXPECT_FALSE(pool.is_searching());
    EXPECT_TRUE(pool.start_search(options));
    ASSERT_TRUE(wait_for_worker_running());
    EXPECT_TRUE(pool.is_searching());

    pool.request_stop();
    pool.wait();

    EXPECT_FALSE(pool.is_searching());
}

TEST_F(ThreadPoolTest, MainWorkerReleasesHelperSearches) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(options));
    pool.wait();

    bool helper_searched = false;
    for (size_t index = 1; index < pool.thread_count(); ++index) {
        helper_searched |= ThreadTestAccess::node_count(pool, index) > 0;
    }

    EXPECT_TRUE(helper_searched);
}

TEST_F(ThreadPoolTest, StartSearchRejectsConcurrentSearch) {
    EXPECT_TRUE(pool.start_search(options));
    ASSERT_TRUE(wait_for_worker_running());
    const auto age = tt.current_age();

    EXPECT_FALSE(pool.start_search(options));
    EXPECT_EQ(tt.current_age(), age);

    pool.request_stop();
    pool.wait();
}

TEST_F(ThreadPoolTest, RequestStopStopsSearch) {
    EXPECT_TRUE(pool.start_search(options));
    ASSERT_TRUE(wait_for_worker_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.request_stop();

    EXPECT_NO_THROW(pool.wait());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, RequestStopImmediatelyAfterStartDoesNotDeadlock) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(options));
    pool.request_stop();
    EXPECT_NO_THROW(pool.wait());

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, RequestStopWhileIdleDoesNotPoisonNextSearch) {
    pool.request_stop();
    EXPECT_NO_THROW(pool.wait());

    options.depth = 1;
    EXPECT_TRUE(pool.start_search(options));
    pool.wait();

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, ShutdownStopsSearch) {
    EXPECT_TRUE(pool.start_search(options));
    ASSERT_TRUE(wait_for_worker_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.shutdown();

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, ShutdownImmediatelyAfterStartDoesNotDeadlock) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(options));
    EXPECT_NO_THROW(pool.shutdown());

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, DestructorShutsDownActiveSearch) {
    options.depth = 5;

    {
        ThreadPool local_pool{THREAD_COUNT, protocol};
        EXPECT_TRUE(local_pool.start_search(options));
    }

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, NodesSearchedAggregatesThreadCounters) {
    options.depth = 1;

    EXPECT_TRUE(pool.start_search(options));
    pool.wait();

    EXPECT_GT(nodes_searched(), 0);
}

TEST_F(ThreadPoolTest, NodeLimitedSearchUsesThreadSafeNodeCount) {
    options.depth = 5;
    options.nodes = 1;

    EXPECT_TRUE(pool.start_search(options));
    pool.wait();

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
    EXPECT_GE(nodes_searched(), static_cast<uint64_t>(options.nodes));
}

TEST_F(ThreadPoolTest, RootSearchAgesSharedTTOncePerStartSearch) {
    options.depth = 1;

    EXPECT_EQ(tt.current_age(), uint8_t{0});

    EXPECT_TRUE(pool.start_search(options));
    pool.wait();
    EXPECT_EQ(tt.current_age(), uint8_t{1});

    EXPECT_TRUE(pool.start_search(options));
    pool.wait();
    EXPECT_EQ(tt.current_age(), uint8_t{2});
}

TEST_F(ThreadPoolTest, ResizeRejectsWhileSearchInProgress) {
    EXPECT_TRUE(pool.start_search(options));
    ASSERT_TRUE(wait_for_worker_running());

    EXPECT_FALSE(pool.resize(THREAD_COUNT + 1));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);

    pool.request_stop();
    pool.wait();
}

TEST_F(ThreadPoolTest, ResizeRejectsAfterShutdown) {
    pool.shutdown();

    EXPECT_FALSE(pool.resize(THREAD_COUNT + 1));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);
}

TEST_F(ThreadPoolTest, ShutdownIsIdempotent) {
    pool.shutdown();
    pool.shutdown();

    EXPECT_FALSE(pool.start_search(options));
    EXPECT_FALSE(pool.resize(THREAD_COUNT + 1));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);
}

TEST_F(ThreadPoolTest, ResizeGrowsAndShrinksIdlePool) {
    options.depth = 1;

    ASSERT_TRUE(pool.resize(THREAD_COUNT + 2));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT + 2);
    EXPECT_TRUE(pool.start_search(options));
    pool.wait();
    EXPECT_TRUE(has_bestmove_output()) << oss.str();

    oss.str("");
    oss.clear();

    ASSERT_TRUE(pool.resize(1));
    EXPECT_EQ(pool.thread_count(), 1U);
    EXPECT_TRUE(pool.start_search(options));
    pool.wait();
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, ResizeToZeroThenBackUp) {
    options.depth = 1;

    ASSERT_TRUE(pool.resize(0));
    EXPECT_EQ(pool.thread_count(), 0U);
    EXPECT_FALSE(pool.start_search(options));

    ASSERT_TRUE(pool.resize(THREAD_COUNT));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);
    EXPECT_TRUE(pool.start_search(options));
    pool.wait();
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, RootLineSnapshotsPublishOnlyCompletedRequestedDepths) {
    options.depth = 1;

    EXPECT_TRUE(pool.start_search(options));
    pool.wait();

    expect_completed_depths(options.depth);
}

TEST_F(ThreadPoolTest, RootLinesReturnsWorkerSnapshots) {
    options.depth = 1;

    EXPECT_TRUE(pool.start_search(options));
    pool.wait();

    const auto lines = pool.root_lines();
    ASSERT_EQ(lines.size(), pool.thread_count());

    bool saw_completed_depth = false;
    for (size_t index = 0; index < lines.size(); ++index) {
        const RootLine snapshot = ThreadTestAccess::root_snapshot(pool, index);

        EXPECT_EQ(lines[index].best_move, snapshot.best_move);
        EXPECT_EQ(lines[index].value, snapshot.value);
        EXPECT_EQ(lines[index].depth, snapshot.depth);
        EXPECT_EQ(lines[index].completed, snapshot.completed);

        saw_completed_depth |= lines[index].completed_depth();
    }

    EXPECT_TRUE(saw_completed_depth);
}
