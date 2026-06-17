#include "thread.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <thread>

#include "board.hpp"
#include "search_options.hpp"
#include "test_util.hpp"
#include "thread_pool.hpp"
#include "uci.hpp"

class ThreadTest : public ::testing::Test {
protected:
    std::istringstream iss;
    std::ostringstream oss;
    uci::Protocol      protocol{oss, oss};
    ThreadPool         pool{1, protocol};
    Thread*            thread;

    void SetUp() override {
        thread = pool.threads[0].get();
        oss.str("");
    }

    bool wait_for_search_start(std::chrono::milliseconds timeout = std::chrono::milliseconds(200)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (thread->search_started_flag.load(std::memory_order_acquire))
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return thread->search_started_flag.load(std::memory_order_acquire);
    }

    void load_thread_board(Board& board) {
        SearchOptions options(iss, &board);
        thread->set_options(options);
        thread->reset();
    }

    int  thread_ply() const { return thread->ply; }
    bool thread_is_draw() const { return thread->board.is_draw(thread_ply()); }

    void make_thread_move(Move move) {
        thread->board.make(move, thread->position_states[thread->ply + 1]);
        ++thread->ply;
    }

    void unmake_thread_move() {
        thread->board.unmake(thread->position_states[thread->ply - 1]);
        --thread->ply;
    }

    void make_thread_null_move() {
        thread->board.make_null(thread->position_states[thread->ply + 1]);
        ++thread->ply;
    }

    void unmake_thread_null_move() {
        thread->board.unmake_null(thread->position_states[thread->ply - 1]);
        --thread->ply;
    }
};

TEST_F(ThreadTest, ThreadShutsDownCorrectly) {
    TestBoard     board{STARTFEN};
    SearchOptions options(iss, &board);

    // Start the search then shut down
    thread->start(options);
    ASSERT_TRUE(wait_for_search_start());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread->shutdown();

    // Check output for best move
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadTest, ThreadHaltsSearchCorrectly) {
    TestBoard     board{STARTFEN};
    SearchOptions options(iss, &board);

    // Start the search then halt
    thread->start(options);
    ASSERT_TRUE(wait_for_search_start());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    thread->halt();

    // Check output for best move
    thread->wait();
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadTest, ThreadHandlesMultipleSearches) {
    TestBoard     board1{STARTFEN};
    TestBoard     board2{EMPTYFEN};
    SearchOptions options1(iss, &board1);
    SearchOptions options2(iss, &board2);

    // Run the first search
    thread->start(options1);
    thread->halt();

    // Check output for the first search, clear the output stream
    thread->wait();
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
    oss.str("");

    // Run the second search
    thread->start(options2);
    thread->halt();

    // Check output for the second search
    thread->wait();
    EXPECT_NE(oss.str().find("bestmove"), std::string::npos) << oss.str();
}

TEST_F(ThreadTest, NullMoveKeepsThreadPlyInSync) {
    TestBoard board{STARTFEN};
    load_thread_board(board);

    EXPECT_EQ(thread_ply(), 0);

    make_thread_null_move();
    EXPECT_EQ(thread_ply(), 1);

    unmake_thread_null_move();
    EXPECT_EQ(thread_ply(), 0);

    make_thread_move(Move(E2, E4));
    EXPECT_EQ(thread_ply(), 1);

    make_thread_null_move();
    EXPECT_EQ(thread_ply(), 2);

    unmake_thread_null_move();
    EXPECT_EQ(thread_ply(), 1);

    unmake_thread_move();
    EXPECT_EQ(thread_ply(), 0);
}

TEST_F(ThreadTest, RootPositionHistoryFeedsSearchRepetitionAfterSourceReset) {
    TestBoard board("7k/8/8/8/8/8/8/K7 w - - 0 1");

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
    ASSERT_FALSE(board.is_draw());

    load_thread_board(board);
    board.reset(STARTFEN);

    make_thread_move(Move(A1, B1));
    EXPECT_FALSE(thread_is_draw());

    make_thread_move(Move(H8, G8));
    make_thread_move(Move(B1, A1));
    make_thread_move(Move(G8, H8));
    EXPECT_TRUE(thread_is_draw());
}

TEST_F(ThreadTest, ThreadObjectSizeStaysBounded) {
    EXPECT_LT(sizeof(Thread), 65536U);
}

TEST_F(ThreadTest, ThreadShutsDownGracefully) {
    thread->shutdown();
    SUCCEED();
}
