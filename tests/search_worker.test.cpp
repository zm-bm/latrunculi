#include "search_worker.hpp"

#include <gtest/gtest.h>

#include <sstream>

#include "search_options.hpp"
#include "test_util.hpp"
#include "thread_test_access.hpp"
#include "threading.hpp"
#include "uci.hpp"

namespace {

class SearchWorkerTest : public ::testing::Test {
protected:
    std::ostringstream oss;
    uci::Writer        writer{oss, oss};
    ThreadPool         pool{1, writer};

    Thread& test_thread() { return ThreadTestAccess::thread(pool); }

    void load_worker_board(Board& board) {
        SearchOptions options;
        options.board = &board;
        ThreadTestAccess::configure_search(test_thread(), options);
        ThreadTestAccess::reset_search_state(test_thread());
    }

    int  worker_ply() { return ThreadTestAccess::ply(test_thread()); }
    bool worker_is_draw() { return ThreadTestAccess::is_draw(test_thread()); }

    void make_worker_move(Move move) { ThreadTestAccess::make(test_thread(), move); }

    void unmake_worker_move() { ThreadTestAccess::unmake(test_thread()); }

    void make_worker_null_move() { ThreadTestAccess::make_null(test_thread()); }

    void unmake_worker_null_move() { ThreadTestAccess::unmake_null(test_thread()); }
};

} // namespace

TEST_F(SearchWorkerTest, NullMoveKeepsWorkerPlyInSync) {
    TestBoard board{STARTFEN};
    load_worker_board(board);

    EXPECT_EQ(worker_ply(), 0);

    make_worker_null_move();
    EXPECT_EQ(worker_ply(), 1);

    unmake_worker_null_move();
    EXPECT_EQ(worker_ply(), 0);

    make_worker_move(Move(E2, E4));
    EXPECT_EQ(worker_ply(), 1);

    make_worker_null_move();
    EXPECT_EQ(worker_ply(), 2);

    unmake_worker_null_move();
    EXPECT_EQ(worker_ply(), 1);

    unmake_worker_move();
    EXPECT_EQ(worker_ply(), 0);
}

TEST_F(SearchWorkerTest, RootPositionHistoryFeedsSearchRepetitionAfterSourceReset) {
    TestBoard board("7k/8/8/8/8/8/8/K7 w - - 0 1");

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
    ASSERT_FALSE(board.is_draw());

    load_worker_board(board);
    board.reset(STARTFEN);

    make_worker_move(Move(A1, B1));
    EXPECT_FALSE(worker_is_draw());

    make_worker_move(Move(H8, G8));
    make_worker_move(Move(B1, A1));
    make_worker_move(Move(G8, H8));
    EXPECT_TRUE(worker_is_draw());
}
