#include "search/search_worker.hpp"

#include <gtest/gtest.h>

#include <sstream>

#include "board/board.hpp"
#include "search/search_limits.hpp"
#include "support/board_fixtures.hpp"
#include "support/thread_test_access.hpp"
#include "uci/threading.hpp"
#include "uci/uci_writer.hpp"

namespace {

class SearchWorkerTest : public ::testing::Test {
protected:
    std::ostringstream oss;
    uci::Writer        writer{oss, oss};
    ThreadPool         pool{1, writer};

    Thread& test_thread() { return ThreadTestAccess::thread(pool); }

    void load_worker_board(Board& board) {
        ThreadTestAccess::configure_search(test_thread(), board, SearchLimits{});
        ThreadTestAccess::reset_search_state(test_thread());
    }

    int  worker_ply() { return ThreadTestAccess::ply(test_thread()); }
    bool worker_is_draw() { return ThreadTestAccess::is_draw(test_thread()); }

    MoveOrdering& worker_ordering() { return ThreadTestAccess::move_ordering(test_thread()); }

    void make_worker_move(Move move) { ThreadTestAccess::make(test_thread(), move); }

    void unmake_worker_move() { ThreadTestAccess::unmake(test_thread()); }

    void make_worker_null_move() { ThreadTestAccess::make_null(test_thread()); }

    void unmake_worker_null_move() { ThreadTestAccess::unmake_null(test_thread()); }
};

} // namespace

TEST_F(SearchWorkerTest, NullMoveKeepsWorkerPlyInSync) {
    Board board{board_test::fen::start};
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

TEST_F(SearchWorkerTest, RepeatedSearchesAgeQuietAndPreserveContinuationHistory) {
    Board board{board_test::fen::start};
    load_worker_board(board);

    worker_ordering().quiets.reward(WHITE, E2, E4, 4);
    worker_ordering().continuations.reward(WHITE, PAWN, E4, KNIGHT, F6, 4);

    ThreadTestAccess::reset_search_state(test_thread());
    EXPECT_EQ(worker_ordering().quiets.get(WHITE, E2, E4), 8);
    EXPECT_EQ(worker_ordering().continuations.get(WHITE, PAWN, E4, KNIGHT, F6), 16);

    ThreadTestAccess::reset_search_state(test_thread());
    EXPECT_EQ(worker_ordering().quiets.get(WHITE, E2, E4), 4);
    EXPECT_EQ(worker_ordering().continuations.get(WHITE, PAWN, E4, KNIGHT, F6), 16);
}

TEST_F(SearchWorkerTest, RootPositionHistoryFeedsSearchRepetitionAfterSourceReset) {
    Board board(board_test::fen::corner_kings);

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
    ASSERT_FALSE(board.is_draw());

    load_worker_board(board);
    board.load_fen(board_test::fen::start);

    make_worker_move(Move(A1, B1));
    EXPECT_FALSE(worker_is_draw());

    make_worker_move(Move(H8, G8));
    make_worker_move(Move(B1, A1));
    make_worker_move(Move(G8, H8));
    EXPECT_TRUE(worker_is_draw());
}
