#include "uci/threading.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <sstream>
#include <thread>

#include "search/search_limits.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"
#include "support/thread_test_access.hpp"
#include "uci/uci_writer.hpp"

namespace {

constexpr int THREAD_COUNT = 4;

SearchLimits default_limits() {
    return SearchLimits{};
}

class ThreadLifecycleTest : public ::testing::Test {
protected:
    std::ostringstream oss;
    uci::Writer        writer{oss, oss};
    ThreadPool         pool{1, writer};

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
    std::ostringstream  oss;
    uci::Writer         writer{oss, oss};
    ThreadPool          pool{THREAD_COUNT, writer};
    board_test::Harness board{board_test::fen::start};
    SearchLimits        options{default_limits()};

    NodeCount nodes_searched() const { return pool.nodes_searched(); }

    void expect_completed_depths(int depth) {
        bool saw_completed_depth = false;
        for (size_t index = 0; index < pool.thread_count(); ++index) {
            auto snapshot = ThreadTestAccess::root_snapshot(pool, index);
            if (!snapshot.completed) {
                EXPECT_FALSE(snapshot.has_completed_depth());
                continue;
            }

            EXPECT_TRUE(snapshot.has_completed_depth());
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

    bool wait_for_tt_age(std::uint8_t              expected,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (tt.current_age() == expected)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return tt.current_age() == expected;
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
    board_test::Harness board{board_test::fen::start};
    SearchLimits        options = default_limits();

    ThreadTestAccess::start_search(test_thread(), board, options);
    ASSERT_TRUE(wait_for_worker_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ThreadTestAccess::shutdown(test_thread());

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadLifecycleTest, ThreadStopsSearchCorrectly) {
    board_test::Harness board{board_test::fen::start};
    SearchLimits        options = default_limits();

    ThreadTestAccess::start_search(test_thread(), board, options);
    ASSERT_TRUE(wait_for_worker_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ThreadTestAccess::request_stop(test_thread());

    ThreadTestAccess::wait_for_idle(test_thread());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadLifecycleTest, ThreadHandlesMultipleSearches) {
    board_test::Harness board1{board_test::fen::start};
    board_test::Harness board2{board_test::fen::kings_only};
    SearchLimits        options1 = default_limits();
    SearchLimits        options2 = default_limits();

    ThreadTestAccess::start_search(test_thread(), board1, options1);
    ThreadTestAccess::request_stop(test_thread());

    ThreadTestAccess::wait_for_idle(test_thread());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
    oss.str("");
    oss.clear();

    ThreadTestAccess::start_search(test_thread(), board2, options2);
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
    ThreadPool empty_pool{0, writer};

    EXPECT_FALSE(empty_pool.start_search(board, options));
    EXPECT_EQ(tt.current_age(), std::uint8_t{0});
}

TEST_F(ThreadPoolTest, StartSearchRejectsAfterShutdown) {
    pool.shutdown();

    EXPECT_FALSE(pool.start_search(board, options));
    EXPECT_EQ(tt.current_age(), std::uint8_t{0});
}

TEST_F(ThreadPoolTest, StartSearchCompletes) {
    options.depth = 5;
    EXPECT_TRUE(pool.start_search(board, options));

    EXPECT_NO_THROW(pool.wait());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, IsSearchingTracksLifecycle) {
    options.depth = 5;

    EXPECT_FALSE(pool.is_searching());
    EXPECT_TRUE(pool.start_search(board, options));
    ASSERT_TRUE(wait_for_worker_running());
    EXPECT_TRUE(pool.is_searching());

    pool.request_stop();
    pool.wait();

    EXPECT_FALSE(pool.is_searching());
}

TEST_F(ThreadPoolTest, MainWorkerReleasesHelperSearches) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    bool helper_searched = false;
    for (size_t index = 1; index < pool.thread_count(); ++index) {
        helper_searched |= ThreadTestAccess::node_count(pool, index) > 0;
    }

    EXPECT_TRUE(helper_searched);
}

TEST_F(ThreadPoolTest, StartSearchRejectsConcurrentSearch) {
    EXPECT_TRUE(pool.start_search(board, options));
    ASSERT_TRUE(wait_for_worker_running());
    ASSERT_TRUE(wait_for_tt_age(1));
    const auto age = tt.current_age();

    EXPECT_FALSE(pool.start_search(board, options));
    EXPECT_EQ(tt.current_age(), age);

    pool.request_stop();
    pool.wait();
}

TEST_F(ThreadPoolTest, RequestStopStopsSearch) {
    EXPECT_TRUE(pool.start_search(board, options));
    ASSERT_TRUE(wait_for_worker_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.request_stop();

    EXPECT_NO_THROW(pool.wait());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, RequestStopImmediatelyAfterStartDoesNotDeadlock) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.request_stop();
    EXPECT_NO_THROW(pool.wait());

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, RequestStopWhileIdleDoesNotPoisonNextSearch) {
    pool.request_stop();
    EXPECT_NO_THROW(pool.wait());

    options.depth = 1;
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, ShutdownStopsSearch) {
    EXPECT_TRUE(pool.start_search(board, options));
    ASSERT_TRUE(wait_for_worker_running());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.shutdown();

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, ShutdownImmediatelyAfterStartDoesNotDeadlock) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(board, options));
    EXPECT_NO_THROW(pool.shutdown());

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, DestructorShutsDownActiveSearch) {
    options.depth = 5;

    {
        ThreadPool local_pool{THREAD_COUNT, writer};
        EXPECT_TRUE(local_pool.start_search(board, options));
    }

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, NodesSearchedAggregatesThreadCounters) {
    options.depth = 1;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    EXPECT_GT(nodes_searched(), 0);
}

TEST_F(ThreadPoolTest, NodeLimitedSearchUsesThreadSafeNodeCount) {
    options.depth = 5;
    options.nodes = 1;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    EXPECT_TRUE(has_bestmove_output()) << oss.str();
    ASSERT_TRUE(options.nodes.has_value());
    EXPECT_GE(nodes_searched(), *options.nodes);
}

TEST_F(ThreadPoolTest, RootSearchAgesSharedTTOncePerStartSearch) {
    options.depth = 1;

    EXPECT_EQ(tt.current_age(), std::uint8_t{0});

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_EQ(tt.current_age(), std::uint8_t{1});

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_EQ(tt.current_age(), std::uint8_t{2});
}

TEST_F(ThreadPoolTest, ResizeRejectsWhileSearchInProgress) {
    EXPECT_TRUE(pool.start_search(board, options));
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

    EXPECT_FALSE(pool.start_search(board, options));
    EXPECT_FALSE(pool.resize(THREAD_COUNT + 1));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);
}

TEST_F(ThreadPoolTest, ResizeGrowsAndShrinksIdlePool) {
    options.depth = 1;

    ASSERT_TRUE(pool.resize(THREAD_COUNT + 2));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT + 2);
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_TRUE(has_bestmove_output()) << oss.str();

    oss.str("");
    oss.clear();

    ASSERT_TRUE(pool.resize(1));
    EXPECT_EQ(pool.thread_count(), 1U);
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, ResizeToZeroThenBackUp) {
    options.depth = 1;

    ASSERT_TRUE(pool.resize(0));
    EXPECT_EQ(pool.thread_count(), 0U);
    EXPECT_FALSE(pool.start_search(board, options));

    ASSERT_TRUE(pool.resize(THREAD_COUNT));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

TEST_F(ThreadPoolTest, RootLineSnapshotsPublishOnlyCompletedRequestedDepths) {
    options.depth = 1;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    expect_completed_depths(options.depth);
}

TEST_F(ThreadPoolTest, RootSnapshotsReturnsWorkerSnapshots) {
    options.depth = 1;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    const auto snapshots = pool.root_snapshots();
    ASSERT_EQ(snapshots.size(), pool.thread_count());

    bool saw_completed_depth = false;
    for (size_t index = 0; index < snapshots.size(); ++index) {
        const RootLine snapshot = ThreadTestAccess::root_snapshot(pool, index);

        EXPECT_EQ(snapshots[index].root_move, snapshot.root_move);
        EXPECT_EQ(snapshots[index].value, snapshot.value);
        EXPECT_EQ(snapshots[index].depth, snapshot.depth);
        EXPECT_EQ(snapshots[index].completed, snapshot.completed);

        saw_completed_depth |= snapshots[index].has_completed_depth();
    }

    EXPECT_TRUE(saw_completed_depth);
}
