#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "eval/evaluation.hpp"
#include "movegen/moves.hpp"
#include "search/root_line.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

namespace search {

namespace {

PV pv_for(Move first, Move second = NULL_MOVE) {
    PV child;
    if (!second.is_null()) {
        PV empty;
        child.update(second, empty);
    }
    PV pv;
    pv.update(first, child);
    return pv;
}

class RootSearchTest : public ::testing::Test {
protected:
    RecordingSearchReporter reporter;
    ThreadPool              pool{1, reporter};
    Thread&                 thread{SearchThreadTestAccess::thread(pool)};
    Worker&                 worker{SearchThreadTestAccess::worker(thread)};
    Limits                  limits;

    void SetUp() override {
        limits.depth = 4;
        tt.clear();
    }

    void load(const Board& board, int depth = 4) {
        tt.clear();
        limits.depth = depth;
        worker.configure_search(board, limits, SearchClock::now());
        SearchTestAccess::reset(worker);
    }

    Board&                 position() { return SearchTestAccess::board(worker); }
    RootLine&              result() { return SearchTestAccess::accepted_result(worker); }
    std::vector<RootLine>& candidates() { return SearchTestAccess::root_candidates(worker); }

    void build_candidates() { SearchTestAccess::build_root_candidates(worker); }

    void root_search() {
        build_candidates();
        SearchTestAccess::search_root(worker);
    }

    Move find_move(std::string_view move_string) {
        return movegen::UnscoredMoves(position()).next_matching([&](Move move) {
            return move.str() == move_string;
        });
    }

    int progress_count(int depth) const {
        return static_cast<int>(std::ranges::count_if(
            reporter.progress, [depth](const RootLine& line) { return line.depth == depth; }));
    }

#if LATRUNCULI_SEARCH_STATS
    const Counters& counters() { return SearchTestAccess::instrumentation(worker).raw_counters(); }
#endif
};

} // namespace

TEST_F(RootSearchTest, SearchesDrawnRootForLegalMove) {
    Board board{board_test::fen::kings_only};
    load(board, 1);
    ASSERT_TRUE(position().is_draw());

    EXPECT_EQ(worker.search(), scores::draw);
    EXPECT_TRUE(result().completed);
    EXPECT_TRUE(position().is_legal_move(result().root_move));
    EXPECT_GT(worker.node_count(), 1U);
    EXPECT_EQ(worker.root_snapshot().root_move, result().root_move);
}

TEST_F(RootSearchTest, DoesNotStoreRootPositionInTt) {
    Board board{board_test::fen::start};
    load(board, 1);
    const PositionKey root_key = position().tt_key();

    root_search();
    EXPECT_FALSE(tt.probe(root_key).record.has_value());
}

TEST_F(RootSearchTest, ResearchesLateRootAlphaImprovement) {
    Board board{"k7/4r3/8/8/8/3Q4/4p3/K7 w - - 0 1"};
    load(board, 1);
    build_candidates();

    const Move first   = find_move("d3e2");
    const Move winning = find_move("d3d8");
    ASSERT_FALSE(first.is_null());
    ASSERT_FALSE(winning.is_null());

    const auto first_it =
        std::find_if(candidates().begin(), candidates().end(), [&](const RootLine& line) {
            return line.root_move == first;
        });
    const auto winning_it =
        std::find_if(candidates().begin(), candidates().end(), [&](const RootLine& line) {
            return line.root_move == winning;
        });
    ASSERT_NE(first_it, candidates().end());
    ASSERT_NE(winning_it, candidates().end());
    const RootLine first_line   = *first_it;
    const RootLine winning_line = *winning_it;
    candidates()                = {first_line, winning_line};

    ASSERT_TRUE(SearchTestAccess::search_root_window(worker, 1, 1, -scores::inf, scores::inf));
    EXPECT_GT(candidates()[1].value, candidates()[0].value);
    EXPECT_EQ(progress_count(1), 2);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().pvs_researches[1], 0U);
#endif

    // Reuse the same child storage for a full search followed by a failed scout.
    const RootLine best = candidates()[1];
    candidates()        = {best, first_line};
    auto& child         = SearchTestAccess::frames(worker)[1];
    child.pv            = pv_for(Move(E2, E4), Move(E7, E5));
    ASSERT_TRUE(SearchTestAccess::search_root_window(worker, 1, 1, -scores::inf, scores::inf));
    EXPECT_LT(candidates()[1].value, candidates()[0].value);
    EXPECT_EQ(candidates()[0].pv, best.pv);
    ASSERT_EQ(candidates()[1].pv.size(), 1);
    EXPECT_EQ(candidates()[1].pv.front(), first);
    EXPECT_FALSE(child.pv_enabled);
    EXPECT_TRUE(child.pv.empty());
}

TEST_F(RootSearchTest, BuildsLegalRootCandidates) {
    Board board{board_test::fen::start};
    load(board, 1);

    int expected = 0;
    for (Move move : board_test::pseudo_legal_moves(position())) {
        if (position().is_legal_pseudo_move(move))
            ++expected;
    }

    build_candidates();
    ASSERT_EQ(candidates().size(), static_cast<std::size_t>(expected));
    for (const RootLine& line : candidates()) {
        EXPECT_TRUE(position().is_legal_pseudo_move(line.root_move));
        EXPECT_FALSE(line.completed);
    }
}

TEST_F(RootSearchTest, CompletesOrdersAndPublishesPV) {
    Board board{board_test::fen::start};
    load(board, 2);

    (void)worker.search();

    ASSERT_TRUE(result().completed);
    ASSERT_TRUE(position().is_legal_move(result().root_move));
    ASSERT_EQ(result().depth, 2);
    ASSERT_EQ(result().pv.size(), 2);
    EXPECT_EQ(result().pv.front(), result().root_move);
    ASSERT_FALSE(candidates().empty());
    EXPECT_EQ(candidates().front(), result());
    for (std::size_t i = 1; i < candidates().size(); ++i)
        EXPECT_FALSE(is_better_root_line(candidates()[i], candidates()[i - 1]));
    EXPECT_EQ(worker.root_snapshot(), result());
}

TEST_F(RootSearchTest, SuppressesOnlyIdenticalProgressReports) {
    Board board{board_test::fen::start};
    load(board, 2);

    RootLine line{
        .root_move = Move(E2, E4),
        .value     = 20,
        .depth     = 1,
        .completed = true,
        .pv        = pv_for(Move(E2, E4)),
    };

    SearchTestAccess::report_root_progress(worker, line);
    SearchTestAccess::report_root_progress(worker, line);
    EXPECT_EQ(progress_count(1), 1);

    ++line.value;
    SearchTestAccess::report_root_progress(worker, line);
    line.pv = pv_for(Move(E2, E4), Move(E7, E5));
    SearchTestAccess::report_root_progress(worker, line);
    line.depth = 2;
    SearchTestAccess::report_root_progress(worker, line);

    EXPECT_EQ(progress_count(1), 3);
    EXPECT_EQ(progress_count(2), 1);
}

TEST_F(RootSearchTest, WidensAspirationWindowAfterFailLowAndFailHigh) {
    struct Case {
        const char* name;
        Score       previous;
        bool        fail_low;
    };

    constexpr std::array cases{
        Case{"fail high", -1000, false},
        Case{"fail low", 1000, true},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{board_test::fen::start};
        load(board, 1);
        build_candidates();

        ASSERT_TRUE(SearchTestAccess::search_root_depth(worker, 1, tc.previous));
        EXPECT_TRUE(result().completed);
        EXPECT_TRUE(position().is_legal_move(result().root_move));
        EXPECT_EQ(result().depth, 1);
        ASSERT_FALSE(result().pv.empty());
        EXPECT_EQ(result().pv.front(), result().root_move);

#if LATRUNCULI_SEARCH_STATS
        if (tc.fail_low) {
            EXPECT_GT(counters().aspiration_fail_lows, 0U);
            EXPECT_EQ(counters().aspiration_fail_highs, 0U);
        } else {
            EXPECT_GT(counters().aspiration_fail_highs, 0U);
            EXPECT_EQ(counters().aspiration_fail_lows, 0U);
        }
#endif
    }
}

TEST_F(RootSearchTest, ReducedAspirationRetryPublishesNominalDepth) {
    Board board{board_test::fen::start};
    load(board, 3);
    build_candidates();

    ASSERT_TRUE(SearchTestAccess::search_root_depth(worker, 3, -1000));
    EXPECT_TRUE(result().completed);
    EXPECT_EQ(result().depth, 3);
    EXPECT_EQ(worker.root_snapshot().depth, 3);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().aspiration_fail_highs, 0U);
#endif
}

TEST_F(RootSearchTest, StoppedAspirationPreservesLastAcceptedSnapshot) {
    Board board{board_test::fen::start};
    load(board, 2);
    build_candidates();
    ASSERT_TRUE(SearchTestAccess::search_root_depth(worker, 1, eval::evaluate(position())));
    const RootLine accepted = worker.root_snapshot();

    SearchThreadTestAccess::request_stop(thread);
    EXPECT_FALSE(SearchTestAccess::search_root_depth(worker, 2, accepted.value));
    EXPECT_EQ(worker.root_snapshot(), accepted);
    EXPECT_EQ(progress_count(2), 0);
}

TEST_F(RootSearchTest, HandlesMateInOneCheckmateAndStalemate) {
    struct Case {
        const char* fen;
        Score       value;
        const char* move;
        std::size_t pv_size;
    };

    constexpr std::array cases{
        Case{"7R/8/8/8/8/1K6/8/1k6 w - - 0 1", scores::mate - 1, "h8h1", 1},
        Case{"7k/8/5KQ1/8/8/8/8/8 w - - 98 1", scores::mate - 1, "g6g7", 1},
        Case{"7k/8/5KQ1/8/8/8/8/8 w - - 99 1", scores::mate - 1, "g6g7", 1},
        Case{"7k/6Q1/6K1/8/8/8/8/8 b - - 0 1", -scores::mate, "none", 0},
        Case{board_test::fen::stalemate, scores::draw, "none", 0},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.fen);
        Board board{tc.fen};
        load(board, 2);
        EXPECT_EQ(worker.search(), tc.value);

        const RootLine snapshot = worker.root_snapshot();
        EXPECT_TRUE(snapshot.completed);
        EXPECT_EQ(snapshot.value, tc.value);
        EXPECT_EQ(snapshot.root_move.str(), tc.move);
        EXPECT_EQ(snapshot.pv.size(), tc.pv_size);
    }
}

TEST_F(RootSearchTest, TakesLooseRookAtDepthOne) {
    Board board{"k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1"};
    load(board, 1);

    const Score static_score = eval::evaluate(board);
    EXPECT_GT(worker.search(), static_score);
    EXPECT_EQ(worker.root_snapshot().root_move.str(), "d1e2");
}

TEST_F(RootSearchTest, StoppedSearchPreservesLastCompletedDepth) {
    Board board{board_test::fen::start};
    limits.depth = 8;
    limits.nodes = 100;

    SearchThreadTestAccess::start_search(thread, board, limits);
    SearchThreadTestAccess::wait_for_idle(thread);

    const RootLine snapshot = worker.root_snapshot();
    ASSERT_TRUE(snapshot.has_completed_depth());
    EXPECT_TRUE(snapshot.usable_root_move());
    EXPECT_LT(snapshot.depth, limits.depth);
    ASSERT_FALSE(snapshot.pv.empty());
    EXPECT_EQ(snapshot.pv.front(), snapshot.root_move);
}

} // namespace search
