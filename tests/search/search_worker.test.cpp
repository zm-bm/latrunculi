#include "search/search_worker.hpp"

#include <array>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "search/search_limits.hpp"
#include "search/search_thread_pool.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

namespace {

class SearchWorkerTest : public ::testing::Test {
protected:
    RecordingSearchReporter reporter;
    SearchThreadPool        pool{1, reporter};

    SearchThread& test_thread() { return SearchThreadTestAccess::thread(pool); }

    SearchWorker& worker() { return SearchThreadTestAccess::worker(test_thread()); }

    void load_worker_board(Board& board) {
        worker().configure_search(board, SearchLimits{}, SearchClock::now());
        SearchTestAccess::reset(worker());
    }

    int& search_ply() { return SearchTestAccess::search_ply(worker()); }
    bool worker_is_draw() { return SearchTestAccess::board(worker()).is_draw(search_ply()); }

    MoveOrdering& worker_ordering() { return SearchTestAccess::ordering(worker()); }

    void make_worker_move(Move move) {
        SearchTestAccess::board(worker()).make(move);
        ++search_ply();
    }

    void unmake_worker_move() {
        SearchTestAccess::board(worker()).unmake();
        --search_ply();
    }

    void make_worker_null_move() {
        SearchTestAccess::board(worker()).make_null();
        ++search_ply();
    }

    void unmake_worker_null_move() {
        SearchTestAccess::board(worker()).unmake_null();
        --search_ply();
    }
};

} // namespace

TEST_F(SearchWorkerTest, NullMoveKeepsSearchPlyInSync) {
    Board board{board_test::fen::start};
    load_worker_board(board);

    EXPECT_EQ(search_ply(), 0);

    make_worker_null_move();
    EXPECT_EQ(search_ply(), 1);

    unmake_worker_null_move();
    EXPECT_EQ(search_ply(), 0);

    make_worker_move(Move(E2, E4));
    EXPECT_EQ(search_ply(), 1);

    make_worker_null_move();
    EXPECT_EQ(search_ply(), 2);

    unmake_worker_null_move();
    EXPECT_EQ(search_ply(), 1);

    unmake_worker_move();
    EXPECT_EQ(search_ply(), 0);
}

TEST_F(SearchWorkerTest, HelperDepthsFollowStaggeringSchedule) {
    constexpr std::array<std::array<bool, 8>, 4> expected{{
        {true, true, true, true, true, true, true, true},
        {true, true, false, true, false, true, false, true},
        {true, false, true, false, true, false, true, false},
        {true, false, false, true, true, false, false, true},
    }};

    SearchThreadPool helper_pool{expected.size(), reporter};
    for (size_t worker_id = 0; worker_id < expected.size(); ++worker_id) {
        const SearchWorker& search_worker = SearchThreadTestAccess::worker(helper_pool, worker_id);
        for (int depth = 1; depth <= static_cast<int>(expected[worker_id].size()); ++depth) {
            EXPECT_EQ(SearchTestAccess::should_search_root_depth(search_worker, depth),
                      expected[worker_id][depth - 1])
                << "worker " << worker_id << ", depth " << depth;
        }
    }
}

TEST_F(SearchWorkerTest, RepeatedSearchesAgeQuietAndPreserveContinuationHistory) {
    Board board{board_test::fen::start};
    load_worker_board(board);

    worker_ordering().quiets.reward(WHITE, E2, E4, 4);
    worker_ordering().continuations.reward(WHITE, PAWN, E4, KNIGHT, F6, 4);

    SearchTestAccess::reset(worker());
    EXPECT_EQ(worker_ordering().quiets.get(WHITE, E2, E4), 8);
    EXPECT_EQ(worker_ordering().continuations.get(WHITE, PAWN, E4, KNIGHT, F6), 16);

    SearchTestAccess::reset(worker());
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

TEST_F(SearchWorkerTest, StoppedSearchReportsFallbackWithoutCompletingRootSnapshot) {
    Board board{board_test::fen::quiet_black_to_move};
    load_worker_board(board);

    SearchThreadTestAccess::request_stop(test_thread());
    SearchThreadTestAccess::wake_for_search(test_thread());
    SearchThreadTestAccess::wait_for_idle(test_thread());

    const RootLine snapshot = worker().root_snapshot();
    EXPECT_FALSE(snapshot.completed);
    EXPECT_EQ(snapshot.root_move, NULL_MOVE);
    EXPECT_EQ(snapshot.depth, 0);
    EXPECT_TRUE(snapshot.pv.empty());

    const auto& root_lines = SearchTestAccess::root_lines(worker());
    ASSERT_FALSE(root_lines.empty());
    ASSERT_FALSE(reporter.progress.empty());
    const RootLine& final_line = reporter.progress.back();
    EXPECT_EQ(final_line.depth, 0);
    EXPECT_TRUE(final_line.pv.empty());
    ASSERT_EQ(reporter.best_moves.size(), 1U);
    EXPECT_EQ(reporter.best_moves.front(), root_lines.front().root_move);
}
