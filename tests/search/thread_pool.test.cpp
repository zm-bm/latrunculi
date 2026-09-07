#include "search/thread_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "board/board.hpp"
#include "search/limits.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_reporter.hpp"
#include "support/search_thread_test_access.hpp"

namespace search {

namespace {

constexpr int THREAD_COUNT = 4;

Limits default_limits() {
    return Limits{};
}

class GatedSearchReporter final : public Reporter {
public:
    void wait_for_best_move() { best_move_observed.acquire(); }
    void release_best_move() { best_move_released.release(); }
    int  best_move_count() const { return reported_best_moves.load(); }

    void report_progress(const RootLine&, const Board&, NodeCount, Milliseconds) override {}

    void report_best_move(Move) override {
        reported_best_moves.fetch_add(1);
        if (!gated_best_move) {
            gated_best_move = true;
            best_move_observed.release();
            best_move_released.acquire();
        }
    }

    void report_diagnostic(std::string_view) override {}

private:
    std::binary_semaphore best_move_observed{0};
    std::binary_semaphore best_move_released{0};
    std::atomic<int>      reported_best_moves{0};
    bool                  gated_best_move{false};
};

class SearchThreadPoolTest : public ::testing::Test {
protected:
    RecordingSearchReporter reporter;
    ThreadPool              pool{THREAD_COUNT, reporter};
    Board                   board{board_test::fen::start};
    Limits                  options{default_limits()};

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
        reporter.clear();
        tt.clear();
    }

    int best_move_count() const { return static_cast<int>(reporter.best_moves.size()); }
};

} // namespace

TEST_F(SearchThreadPoolTest, StartSearchRejectsEmptyPool) {
    ThreadPool empty_pool{0, reporter};

    EXPECT_FALSE(empty_pool.start_search(board, options));
    EXPECT_EQ(tt.current_generation(), std::uint8_t{0});
}

TEST_F(SearchThreadPoolTest, StartSearchCompletes) {
    options.depth = 5;
    EXPECT_TRUE(pool.start_search(board, options));

    EXPECT_NO_THROW(pool.wait());
    EXPECT_EQ(best_move_count(), 1);
}

TEST(SearchThreadPoolTransitionTest, ImmediateRestartAfterBestMovePublicationIsAccepted) {
    GatedSearchReporter reporter;
    ThreadPool          pool{2, reporter};
    Board               board{board_test::fen::start};
    Limits              limits;
    limits.depth = 1;
    tt.clear();

    ASSERT_TRUE(pool.start_search(board, limits));
    reporter.wait_for_best_move();

    const bool   publication_holds_state_lock = SearchThreadTestAccess::state_lock_is_held(pool);
    bool         next_search_accepted         = false;
    std::jthread next_search([&] { next_search_accepted = pool.start_search(board, limits); });

    reporter.release_best_move();
    next_search.join();
    pool.wait();

    EXPECT_TRUE(publication_holds_state_lock);
    EXPECT_TRUE(next_search_accepted);
    EXPECT_EQ(reporter.best_move_count(), 2);
}

#if LATRUNCULI_SEARCH_STATS
TEST_F(SearchThreadPoolTest, ReportsAggregatedSearchInstrumentation) {
    options.depth = 2;
    ASSERT_TRUE(pool.start_search(board, options));
    pool.wait();

    ASSERT_EQ(reporter.diagnostics.size(), 1U);
    const std::string& diagnostic = reporter.diagnostics.front();
    EXPECT_NE(diagnostic.find("Aspiration:"), std::string::npos);
    EXPECT_NE(diagnostic.find("RazorFutility:"), std::string::npos);
    EXPECT_NE(diagnostic.find("QuietHistory:"), std::string::npos);
    EXPECT_NE(diagnostic.find("Ply"), std::string::npos);
}
#endif

TEST_F(SearchThreadPoolTest, IsSearchingTracksLifecycle) {
    options.depth = 5;

    EXPECT_FALSE(pool.is_searching());
    EXPECT_TRUE(pool.start_search(board, options));
    EXPECT_TRUE(pool.is_searching());

    pool.request_stop();
    pool.wait();

    EXPECT_FALSE(pool.is_searching());
}

TEST_F(SearchThreadPoolTest, MainWorkerCoordinatesHelperLifecycle) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    bool helper_searched = false;
    for (size_t index = 1; index < pool.thread_count(); ++index) {
        helper_searched |= SearchThreadTestAccess::node_count(pool, index) > 0;
    }

    EXPECT_TRUE(helper_searched);
    EXPECT_FALSE(pool.is_searching());
}

TEST_F(SearchThreadPoolTest, StartSearchRejectsConcurrentSearch) {
    EXPECT_TRUE(pool.start_search(board, options));
    EXPECT_TRUE(pool.is_searching());

    EXPECT_FALSE(pool.start_search(board, options));

    pool.request_stop();
    pool.wait();
    EXPECT_EQ(tt.current_generation(), std::uint8_t{1});
}

TEST_F(SearchThreadPoolTest, RequestStopStopsSearch) {
    EXPECT_TRUE(pool.start_search(board, options));
    ASSERT_TRUE(wait_for_nodes());
    pool.request_stop();

    EXPECT_NO_THROW(pool.wait());
    EXPECT_EQ(best_move_count(), 1);
}

TEST_F(SearchThreadPoolTest, RequestStopImmediatelyAfterStartDoesNotDeadlock) {
    options.depth = 5;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.request_stop();
    EXPECT_NO_THROW(pool.wait());

    ASSERT_EQ(best_move_count(), 1);
    EXPECT_FALSE(reporter.best_moves.front().is_null());
}

TEST_F(SearchThreadPoolTest, RequestStopWhileIdleDoesNotPoisonNextSearch) {
    pool.request_stop();
    EXPECT_NO_THROW(pool.wait());

    options.depth = 1;
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    EXPECT_EQ(best_move_count(), 1);
}

TEST_F(SearchThreadPoolTest, ShutdownPonderSearchDoesNotDeadlock) {
    options.depth  = 5;
    options.ponder = true;

    EXPECT_TRUE(pool.start_search(board, options));
    EXPECT_NO_THROW(pool.shutdown());

    EXPECT_EQ(best_move_count(), 1);
}

TEST_F(SearchThreadPoolTest, DestructorShutsDownActiveSearch) {
    options.depth = 5;

    {
        ThreadPool local_pool{THREAD_COUNT, reporter};
        EXPECT_TRUE(local_pool.start_search(board, options));
    }

    EXPECT_EQ(best_move_count(), 1);
}

TEST_F(SearchThreadPoolTest, NodesSearchedAggregatesThreadCounters) {
    options.depth = 1;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    NodeCount worker_nodes = 0;
    for (size_t index = 0; index < pool.thread_count(); ++index)
        worker_nodes += SearchThreadTestAccess::node_count(pool, index);

    EXPECT_GT(nodes_searched(), 0);
    EXPECT_EQ(nodes_searched(), worker_nodes);
}

TEST_F(SearchThreadPoolTest, NodeLimitedSearchUsesFreshThreadSafeNodeCounts) {
    options.depth = 5;
    options.nodes = 1;

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();

    EXPECT_EQ(best_move_count(), 1);
    ASSERT_TRUE(options.nodes.has_value());
    EXPECT_GE(nodes_searched(), *options.nodes);

    bool helper_searched = false;
    for (size_t index = 1; index < pool.thread_count(); ++index)
        helper_searched |= SearchThreadTestAccess::node_count(pool, index) > 0;
    ASSERT_TRUE(helper_searched);

    for (size_t index = 0; index < pool.thread_count(); ++index) {
        Thread& thread = SearchThreadTestAccess::thread(pool, index);
        SearchThreadTestAccess::configure_search(thread, board, options);
        EXPECT_EQ(SearchThreadTestAccess::node_count(thread), 0);
    }
}

TEST_F(SearchThreadPoolTest, RootSearchAdvancesTTGenerationOncePerSearch) {
    options.depth = 1;

    EXPECT_EQ(tt.current_generation(), std::uint8_t{0});

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_EQ(tt.current_generation(), std::uint8_t{1});

    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_EQ(tt.current_generation(), std::uint8_t{2});
}

TEST_F(SearchThreadPoolTest, ResizeRejectsWhileSearchInProgress) {
    EXPECT_TRUE(pool.start_search(board, options));
    EXPECT_TRUE(pool.is_searching());

    EXPECT_FALSE(pool.resize(THREAD_COUNT + 1));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);

    pool.request_stop();
    pool.wait();
}

TEST_F(SearchThreadPoolTest, ShutdownIsIdempotent) {
    pool.shutdown();
    pool.shutdown();

    EXPECT_FALSE(pool.start_search(board, options));
    EXPECT_FALSE(pool.resize(THREAD_COUNT + 1));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);
}

TEST_F(SearchThreadPoolTest, ResizeGrowsAndShrinksIdlePool) {
    options.depth = 1;

    EXPECT_THROW(pool.resize(std::numeric_limits<size_t>::max()), std::length_error);
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);

    ASSERT_TRUE(pool.resize(THREAD_COUNT + 2));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT + 2);
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_EQ(best_move_count(), 1);

    reporter.clear();

    ASSERT_TRUE(pool.resize(1));
    EXPECT_EQ(pool.thread_count(), 1U);
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_EQ(best_move_count(), 1);
}

TEST_F(SearchThreadPoolTest, ResizeToZeroThenBackUp) {
    options.depth = 1;

    ASSERT_TRUE(pool.resize(0));
    EXPECT_EQ(pool.thread_count(), 0U);
    EXPECT_FALSE(pool.start_search(board, options));

    ASSERT_TRUE(pool.resize(THREAD_COUNT));
    EXPECT_EQ(pool.thread_count(), THREAD_COUNT);
    EXPECT_TRUE(pool.start_search(board, options));
    pool.wait();
    EXPECT_EQ(best_move_count(), 1);
}

} // namespace search
