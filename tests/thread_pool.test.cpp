#include "thread_pool.hpp"

#include <chrono>
#include <sstream>
#include <thread>

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
    int      initial_depth(int thread_index) {
        return pool.threads[thread_index]->initial_search_depth();
    }
    Move best_voted_move(Move fallback) { return pool.best_voted_move(board, fallback); }

    void set_root_result(int thread_index, RootSearchResult result) {
        auto&                       thread = *pool.threads[thread_index];
        std::lock_guard<std::mutex> lock(thread.root_result_mutex);
        thread.root_result = result;
    }

    bool search_started() {
        for (const auto& thread : pool.threads) {
            if (thread->search_started_flag.load(std::memory_order_acquire))
                return true;
        }
        return false;
    }

    bool wait_for_search_start(std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (search_started())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return search_started();
    }

    void SetUp() override {
        oss.str("");
        oss.clear();
        tt.clear();
    }
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
    ASSERT_TRUE(wait_for_search_start());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.halt_all();

    // Check output for best move
    EXPECT_NO_THROW(pool.wait_all());
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadPoolTest, ShutdownAllThreads) {
    // Start the pool and then shutdown all threads
    pool.start_all(options);
    ASSERT_TRUE(wait_for_search_start());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.shutdown_all();

    // Check output for best move
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadPoolTest, AccumulateNodes) {
    options.depth = 1;

    pool.start_all(options);
    pool.wait_all();

    EXPECT_GT(accumulate_nodes(), 0);
}

TEST_F(ThreadPoolTest, RootSearchAgesSharedTTOncePerStartAll) {
    options.depth = 1;

    EXPECT_EQ(tt.current_age(), uint8_t{0});

    pool.start_all(options);
    pool.wait_all();
    EXPECT_EQ(tt.current_age(), uint8_t{1});

    pool.start_all(options);
    pool.wait_all();
    EXPECT_EQ(tt.current_age(), uint8_t{2});
}

TEST_F(ThreadPoolTest, HelperThreadsUseIntentionalDepthOffset) {
    EXPECT_EQ(initial_depth(0), 1);
    EXPECT_EQ(initial_depth(1), 2);
    EXPECT_EQ(initial_depth(2), 1);
    EXPECT_EQ(initial_depth(3), 2);
}

TEST_F(ThreadPoolTest, BestVotedMovePrefersDeeperCompletedLegalResult) {
    Move main_move{E2, E4};
    Move helper_move{G1, F3};

    set_root_result(0, {main_move, 100, 4, 1000, true});
    set_root_result(1, {helper_move, 0, 5, 500, true});

    EXPECT_EQ(best_voted_move(main_move), helper_move);
}

TEST_F(ThreadPoolTest, BestVotedMoveIgnoresIncompleteNullAndIllegalResults) {
    Move fallback{E2, E4};
    Move incomplete{G1, F3};
    Move illegal_empty_square_move{A1, A8};

    set_root_result(0, {fallback, 10, 3, 100, true});
    set_root_result(1, {incomplete, 1000, 8, 1000, false});
    set_root_result(2, {NULL_MOVE, 1000, 8, 1000, true});
    set_root_result(3, {illegal_empty_square_move, 1000, 8, 1000, true});

    EXPECT_EQ(best_voted_move(fallback), fallback);
}
