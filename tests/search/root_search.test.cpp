#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "eval/evaluator.hpp"
#include "movegen/movegen.hpp"
#include "search/root_line.hpp"
#include "search/search_worker.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_test_access.hpp"
#include "support/thread_test_access.hpp"
#include "uci/threading.hpp"
#include "uci/uci_writer.hpp"

namespace {

PrincipalVariation pv_for(Move first, Move second = NULL_MOVE) {
    PrincipalVariation child;
    if (!second.is_null()) {
        PrincipalVariation empty;
        child.update(second, empty);
    }
    PrincipalVariation pv;
    pv.update(first, child);
    return pv;
}

class RootSearchTest : public ::testing::Test {
protected:
    std::ostringstream output;
    uci::Writer        writer{output, output};
    ThreadPool         pool{1, writer};
    Thread&            thread{ThreadTestAccess::thread(pool)};
    SearchWorker&      worker{ThreadTestAccess::worker(thread)};
    SearchLimits       limits;

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
    int&                   ply() { return SearchTestAccess::search_ply(worker); }
    RootLine&              result() { return SearchTestAccess::root_result(worker); }
    std::vector<RootLine>& lines() { return SearchTestAccess::root_lines(worker); }

    void build_lines() { SearchTestAccess::build_root_lines(worker); }

    EvalValue root_search() {
        build_lines();
        return SearchTestAccess::search_root(worker);
    }

    Move find_move(std::string_view move_string) {
        for (Move move : movegen::generate_pseudo_legal(position())) {
            if (move.str() == move_string && position().is_legal_pseudo_move(move))
                return move;
        }
        return NULL_MOVE;
    }

    int count_info(std::string_view prefix) const {
        std::istringstream stream{output.str()};
        std::string        line;
        int                count = 0;
        while (std::getline(stream, line)) {
            if (line.starts_with(prefix))
                ++count;
        }
        return count;
    }

#if LATRUNCULI_SEARCH_STATS
    const SearchCounters& counters() {
        return SearchTestAccess::instrumentation(worker).raw_counters();
    }
#endif
};

} // namespace

TEST_F(RootSearchTest, SearchesDrawnRootForLegalMove) {
    Board board{"k7/8/2K5/8/8/8/8/8 b - - 100 1"};
    load(board, 1);
    ASSERT_TRUE(position().is_draw());

    EXPECT_EQ(worker.search(), eval_value::draw);
    EXPECT_TRUE(result().completed);
    EXPECT_TRUE(position().is_legal_move(result().root_move));
    EXPECT_GT(worker.node_count(), 1U);
    EXPECT_EQ(worker.root_snapshot().root_move, result().root_move);
}

TEST_F(RootSearchTest, DoesNotStoreRootPositionInTt) {
    Board board{board_test::fen::start};
    load(board, 1);
    const PositionKey root_key = position().key();

    (void)root_search();
    EXPECT_FALSE(tt.probe(root_key).has_value());
}

TEST_F(RootSearchTest, ResearchesLateRootAlphaImprovement) {
    Board board{"k7/4r3/8/8/8/3Q4/4p3/K7 w - - 0 1"};
    load(board, 1);
    build_lines();

    const Move first   = find_move("d3e2");
    const Move winning = find_move("d3d8");
    ASSERT_FALSE(first.is_null());
    ASSERT_FALSE(winning.is_null());

    const auto first_it   = std::find_if(lines().begin(), lines().end(), [&](const RootLine& line) {
        return line.root_move == first;
    });
    const auto winning_it = std::find_if(lines().begin(), lines().end(), [&](const RootLine& line) {
        return line.root_move == winning;
    });
    ASSERT_NE(first_it, lines().end());
    ASSERT_NE(winning_it, lines().end());
    const RootLine first_line   = *first_it;
    const RootLine winning_line = *winning_it;
    lines()                     = {first_line, winning_line};

    ASSERT_TRUE(SearchTestAccess::search_root_window(worker, 1, -eval_value::inf, eval_value::inf));
    EXPECT_GT(lines()[1].value, lines()[0].value);
    EXPECT_EQ(count_info("info depth 1 "), 2);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().pvs_researches[1], 0U);
#endif
}

TEST_F(RootSearchTest, BuildsLegalRootCandidates) {
    Board board{board_test::fen::start};
    load(board, 1);

    int expected = 0;
    for (Move move : movegen::generate_pseudo_legal(position())) {
        if (position().is_legal_pseudo_move(move))
            ++expected;
    }

    build_lines();
    ASSERT_EQ(lines().size(), static_cast<std::size_t>(expected));
    for (const RootLine& line : lines()) {
        EXPECT_TRUE(position().is_legal_pseudo_move(line.root_move));
        EXPECT_FALSE(line.completed);
    }
}

TEST_F(RootSearchTest, CompletesOrdersAndPublishesPrincipalVariation) {
    Board board{board_test::fen::start};
    load(board, 2);

    (void)worker.search();

    ASSERT_TRUE(result().completed);
    ASSERT_TRUE(position().is_legal_move(result().root_move));
    ASSERT_EQ(result().depth, 2);
    ASSERT_EQ(result().pv.size(), 2);
    EXPECT_EQ(result().pv.front(), result().root_move);
    ASSERT_FALSE(lines().empty());
    EXPECT_EQ(lines().front(), result());
    for (std::size_t i = 1; i < lines().size(); ++i)
        EXPECT_FALSE(is_better_root_line(lines()[i], lines()[i - 1]));
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
    EXPECT_EQ(count_info("info depth 1 "), 1);

    ++line.value;
    SearchTestAccess::report_root_progress(worker, line);
    line.pv = pv_for(Move(E2, E4), Move(E7, E5));
    SearchTestAccess::report_root_progress(worker, line);
    line.depth = 2;
    SearchTestAccess::report_root_progress(worker, line);

    EXPECT_EQ(count_info("info depth 1 "), 3);
    EXPECT_EQ(count_info("info depth 2 "), 1);
}

TEST_F(RootSearchTest, WidensAspirationWindowAfterFailLowAndFailHigh) {
    struct Case {
        const char* name;
        EvalValue   previous;
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
        build_lines();

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

TEST_F(RootSearchTest, StoppedAspirationPreservesLastAcceptedSnapshot) {
    Board board{board_test::fen::start};
    load(board, 2);
    build_lines();
    ASSERT_TRUE(SearchTestAccess::search_root_depth(worker, 1, evaluate(position())));
    const RootLine accepted = worker.root_snapshot();

    ThreadTestAccess::request_stop(thread);
    EXPECT_FALSE(SearchTestAccess::search_root_depth(worker, 2, accepted.value));
    EXPECT_EQ(worker.root_snapshot(), accepted);
    EXPECT_EQ(count_info("info depth 2 "), 0);
}

TEST_F(RootSearchTest, HandlesMateInOneCheckmateAndStalemate) {
    struct Case {
        const char* fen;
        EvalValue   value;
        const char* move;
        std::size_t pv_size;
    };

    constexpr std::array cases{
        Case{"7R/8/8/8/8/1K6/8/1k6 w - - 0 1", eval_value::mate - 1, "h8h1", 1},
        Case{"7k/6Q1/6K1/8/8/8/8/8 b - - 0 1", -eval_value::mate, "none", 0},
        Case{board_test::fen::stalemate, eval_value::draw, "none", 0},
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

TEST_F(RootSearchTest, StoppedSearchPreservesLastCompletedDepth) {
    Board board{board_test::fen::start};
    limits.depth = 8;
    limits.nodes = 100;

    ThreadTestAccess::start_search(thread, board, limits);
    ThreadTestAccess::wait_for_idle(thread);

    const RootLine snapshot = worker.root_snapshot();
    ASSERT_TRUE(snapshot.has_completed_depth());
    EXPECT_TRUE(snapshot.usable_root_move());
    EXPECT_LT(snapshot.depth, limits.depth);
    ASSERT_FALSE(snapshot.pv.empty());
    EXPECT_EQ(snapshot.pv.front(), snapshot.root_move);
}
