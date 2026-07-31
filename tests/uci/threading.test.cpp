#include "uci/threading.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <sstream>
#include <thread>

#include "board/board.hpp"
#include "search/search_limits.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "support/thread_test_access.hpp"
#include "uci/uci_writer.hpp"

namespace {

constexpr int THREAD_COUNT = 4;

SearchLimits default_limits() {
    return SearchLimits{};
}

class ThreadPoolTest : public ::testing::Test {
protected:
    std::ostringstream oss;
    uci::Writer        writer{oss, oss};
    ThreadPool         pool{THREAD_COUNT, writer};
    Board              board{board_test::fen::start};
    SearchLimits       options{default_limits()};

    NodeCount nodes_searched() const { return pool.nodes_searched(); }

    bool wait_for_nodes(std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (nodes_searched() > 0)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return nodes_searched() > 0;
    }

    void SetUp() override {
        oss.str("");
        oss.clear();
        tt.clear();
    }

    bool has_bestmove_output() const { return oss.str().find("bestmove") != std::string::npos; }
};

} // namespace

TEST_F(ThreadPoolTest, StartSearchRejectsEmptyPool) {
    ThreadPool empty_pool{0, writer};

    EXPECT_FALSE(empty_pool.start_search(board, options));
    EXPECT_EQ(tt.current_age(), std::uint8_t{0});
}

TEST_F(ThreadPoolTest, StartSearchCompletes) {
    options.depth = 5;
    EXPECT_TRUE(pool.start_search(board, options));

    EXPECT_NO_THROW(pool.wait());
    EXPECT_TRUE(has_bestmove_output()) << oss.str();
}

#if LATRUNCULI_SEARCH_STATS
TEST_F(ThreadPoolTest, ReportsAggregatedSearchInstrumentation) {
    options.depth = 2;
    ASSERT_TRUE(pool.start_search(board, options));
    pool.wait();

    const std::string transcript = oss.str();
    const auto        report     = transcript.find("Aspiration:");
    ASSERT_NE(report, std::string::npos) << transcript;
    EXPECT_EQ(report, transcript.rfind("Aspiration:")) << transcript;
    EXPECT_NE(transcript.find("RazorFutility:", report), std::string::npos) << transcript;
    EXPECT_NE(transcript.find("QuietHistory:", report), std::string::npos) << transcript;
    EXPECT_NE(transcript.find("Depth", report), std::string::npos) << transcript;
}
#endif

TEST_F(ThreadPoolTest, IsSearchingTracksLifecycle) {
    options.depth = 5;

    EXPECT_FALSE(pool.is_searching());
    EXPECT_TRUE(pool.start_search(board, options));
    EXPECT_TRUE(pool.is_searching());

    pool.request_stop();
    pool.wait();

    EXPECT_FALSE(pool.is_searching());
}

TEST_F(ThreadPoolTest, MainWorkerCoordinatesHelperLifecycle) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    bool helper_searched = false;
    for (size_t index = 1; index < pool.thread_count(); ++index) {
        helper_searched |= ThreadTestAccess::node_count(pool, index) > 0;
    }

    EXPECT_TRUE(helper_searched);
    EXPECT_FALSE(pool.is_searching());
}

TEST_F(ThreadPoolTest, StartSearchRejectsConcurrentSearch) {
    EXPECT_TRUE(pool.start_search(board, options));
    EXPECT_TRUE(pool.is_searching());

    EXPECT_FALSE(pool.start_search(board, options));

    pool.request_stop();
    pool.wait();
    EXPECT_EQ(tt.current_age(), std::uint8_t{1});
}

TEST_F(ThreadPoolTest, RequestStopStopsSearch) {
    EXPECT_TRUE(pool.start_search(board, options));
    ASSERT_TRUE(wait_for_nodes());
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
    EXPECT_EQ(oss.str().find("bestmove 0000"), std::string::npos) << oss.str();
}

TEST_F(ThreadPoolTest, RequestStopWhileIdleDoesNotPoisonNextSearch) {
    pool.request_stop();
    EXPECT_NO_THROW(pool.wait());

    options.depth = 1;
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

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

    NodeCount worker_nodes = 0;
    for (size_t index = 0; index < pool.thread_count(); ++index)
        worker_nodes += ThreadTestAccess::node_count(pool, index);

    EXPECT_GT(nodes_searched(), 0);
    EXPECT_EQ(nodes_searched(), worker_nodes);
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
    EXPECT_TRUE(pool.is_searching());

    EXPECT_FALSE(pool.resize(THREAD_COUNT + 1));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);

    pool.request_stop();
    pool.wait();
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
