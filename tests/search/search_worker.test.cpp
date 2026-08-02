#include "search/search_worker.hpp"

#include <gtest/gtest.h>

#include <array>
#include <sstream>

#include "board/board.hpp"
#include "search/search_limits.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_test_access.hpp"
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

    SearchWorker& worker() { return ThreadTestAccess::worker(test_thread()); }

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

    ThreadPool helper_pool{expected.size(), writer};
    for (size_t worker_id = 0; worker_id < expected.size(); ++worker_id) {
        const SearchWorker& search_worker = ThreadTestAccess::worker(helper_pool, worker_id);
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

    ThreadTestAccess::request_stop(test_thread());
    ThreadTestAccess::wake_for_search(test_thread());
    ThreadTestAccess::wait_for_idle(test_thread());

    const RootLine snapshot = worker().root_snapshot();
    EXPECT_FALSE(snapshot.completed);
    EXPECT_EQ(snapshot.root_move, NULL_MOVE);
    EXPECT_EQ(snapshot.depth, 0);
    EXPECT_TRUE(snapshot.pv.empty());

    const auto& root_lines = SearchTestAccess::root_lines(worker());
    ASSERT_FALSE(root_lines.empty());
    const std::string transcript = oss.str();
    EXPECT_NE(transcript.find("info depth 0 "), std::string::npos);
    EXPECT_EQ(transcript.find(" pv "), std::string::npos);
    EXPECT_NE(transcript.find("bestmove " + root_lines.front().root_move.str()), std::string::npos);
    EXPECT_EQ(transcript.find("bestmove 0000"), std::string::npos);
}
