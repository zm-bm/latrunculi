#include <array>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

namespace search {
namespace {

constexpr auto low_clock_queen_fen  = "7k/8/8/8/8/8/6Q1/6K1 w - - 0 1";
constexpr auto high_clock_queen_fen = "7k/8/8/8/8/8/6Q1/6K1 w - - 98 1";
// All evasions are quiet and reach the fifty-move limit from clock 99.
constexpr auto low_clock_evasion_fen  = "7k/8/8/8/8/8/7Q/6K1 b - - 0 1";
constexpr auto high_clock_evasion_fen = "7k/8/8/8/8/8/7Q/6K1 b - - 99 1";

class TtClockTest : public ::testing::Test {
protected:
    RecordingSearchReporter reporter;
    ThreadPool              pool{1, reporter};
    Worker&                 worker{SearchThreadTestAccess::worker(pool)};

    void SetUp() override { tt.resize(32); }
    void TearDown() override { tt.clear(); }

    void load(const Board& board) {
        Limits limits;
        limits.depth = 6;
        worker.configure_search(board, limits, SearchClock::now());
        SearchTestAccess::reset(worker);
    }

    EvalValue root(const char* fen) {
        load(Board(fen));
        return worker.search();
    }

    EvalValue search(bool pv, int depth, EvalValue alpha, EvalValue beta) {
        if (pv)
            return SearchTestAccess::alphabeta<NodeType::Pv>(worker, alpha, beta, depth);
        return SearchTestAccess::alphabeta<NodeType::NonPv>(worker, alpha, beta, depth);
    }
};

TEST_F(TtClockTest, RetainedLowClockSearchCannotHideImminentDraw) {
    for (int repeat = 0; repeat < 3; ++repeat) {
        SCOPED_TRACE(repeat);
        tt.clear();
        EXPECT_EQ(root(high_clock_queen_fen), eval_value::draw);
        tt.clear();
        EXPECT_GT(root(low_clock_queen_fen), 1000);
        EXPECT_EQ(root(high_clock_queen_fen), eval_value::draw);
        tt.clear(); // Clear only the TT, preserving search heuristics.
        EXPECT_EQ(root(high_clock_queen_fen), eval_value::draw);
        EXPECT_GT(root(low_clock_queen_fen), 1000);
    }
}

TEST_F(TtClockTest, MainAndQuiescenceSeparateLowAndNearLimitBounds) {
    for (int depth : {0, 2}) {
        for (bool pv : {false, true}) {
            SCOPED_TRACE(testing::Message() << "depth=" << depth << " pv=" << pv);
            tt.clear();
            load(Board(low_clock_evasion_fen));
            const auto low = search(pv, depth, -eval_value::inf, eval_value::inf);
            EXPECT_LT(low, -1000);
            load(Board(high_clock_evasion_fen));
            EXPECT_EQ(search(pv, depth, -eval_value::inf, eval_value::inf), eval_value::draw);
            tt.clear();
            load(Board(high_clock_evasion_fen));
            EXPECT_EQ(search(pv, depth, -eval_value::inf, eval_value::inf), eval_value::draw);
            load(Board(low_clock_evasion_fen));
            EXPECT_LT(search(pv, depth, -eval_value::inf, eval_value::inf), -1000);
        }
    }
}

TEST_F(TtClockTest, RejectsMismatchedExactLowerUpperAndMateBoundsInBothSearches) {
    struct Case {
        TTBound   bound;
        EvalValue score;
        EvalValue alpha;
        EvalValue beta;
    };
    constexpr std::array cases{
        Case{TTBound::Exact, 20000, -eval_value::inf, eval_value::inf},
        Case{TTBound::LowerBound, 20000, 9000, 10000},
        Case{TTBound::UpperBound, -20000, -10000, -9000},
        Case{TTBound::Exact, eval_value::mate - 10, -eval_value::inf, eval_value::inf},
        Case{TTBound::Exact, -eval_value::mate + 10, -eval_value::inf, eval_value::inf},
    };
    for (int depth : {0, 2}) {
        for (bool pv : {false, true}) {
            for (bool to_high : {false, true}) {
                const Board source(to_high ? low_clock_evasion_fen : high_clock_evasion_fen);
                const Board target(to_high ? high_clock_evasion_fen : low_clock_evasion_fen);
                for (const auto& tc : cases) {
                    if (pv && tc.bound != TTBound::Exact)
                        continue;
                    SCOPED_TRACE(testing::Message()
                                 << "depth=" << depth << " pv=" << pv << " to_high=" << to_high
                                 << " score=" << tc.score << " bound=" << int(tc.bound));
                    tt.clear();
                    load(target);
                    const auto cold = search(pv, depth, tc.alpha, tc.beta);

                    tt.clear();
                    tt.store(source.tt_key(), NULL_MOVE, tc.score, depth, tc.bound, 0);
                    load(target);
                    EXPECT_EQ(search(pv, depth, tc.alpha, tc.beta), cold);
                    EXPECT_GT(worker.node_count(), 1U);

                    // Positive control: the same record must still cut off at a matching clock.
                    tt.clear();
                    tt.store(target.tt_key(), NULL_MOVE, tc.score, depth, tc.bound, 0);
                    load(target);
                    EXPECT_EQ(search(pv, depth, tc.alpha, tc.beta), tc.score);
                    EXPECT_EQ(worker.node_count(), 1U);
                }
            }
        }
    }
}

TEST_F(TtClockTest, PreservesMatesBeforeTheFiftyMoveLimit) {
    constexpr auto low_mate  = "7k/8/5KQ1/8/8/8/8/8 w - - 0 1";
    constexpr auto high_mate = "7k/8/5KQ1/8/8/8/8/8 w - - 98 1";
    EXPECT_EQ(root(low_mate), eval_value::mate - 1);
    EXPECT_EQ(root(high_mate), eval_value::mate - 1);
    tt.clear();
    EXPECT_EQ(root(high_mate), eval_value::mate - 1);
    EXPECT_EQ(root(low_mate), eval_value::mate - 1);

    for (int depth : {0, 2}) {
        for (bool pv : {false, true}) {
            tt.clear();
            load(Board("7k/6Q1/6K1/8/8/8/8/8 b - - 99 1"));
            EXPECT_EQ(search(pv, depth, -eval_value::inf, eval_value::inf), -eval_value::mate);
        }
    }
}

TEST_F(TtClockTest, ClockGroupsControlMainAndQuiescenceCutoffs) {
    struct Clocks {
        int  stored;
        int  current;
        bool share;
    };
    constexpr std::array cases{Clocks{0, 15, true},
                               Clocks{8, 10, true},
                               Clocks{15, 16, false},
                               Clocks{16, 23, true},
                               Clocks{23, 24, false},
                               Clocks{72, 79, true},
                               Clocks{79, 80, false},
                               Clocks{80, 81, false},
                               Clocks{98, 99, false},
                               Clocks{80, 80, true},
                               Clocks{99, 99, true}};
    for (auto clocks : cases) {
        for (bool reverse : {false, true}) {
            if (reverse)
                std::swap(clocks.stored, clocks.current);
            const auto fen = [](int clock) {
                return "7k/8/8/8/8/8/7Q/6K1 b - - " + std::to_string(clock) + " 1";
            };
            for (int depth : {0, 2}) {
                for (bool pv : {false, true}) {
                    SCOPED_TRACE(testing::Message() << clocks.stored << " -> " << clocks.current
                                                    << " depth=" << depth << " pv=" << pv);
                    for (TTBound bound :
                         {TTBound::Exact, TTBound::LowerBound, TTBound::UpperBound}) {
                        if (pv && bound != TTBound::Exact)
                            continue;
                        const EvalValue score = bound == TTBound::UpperBound ? -20000 : 20000;
                        const EvalValue alpha = bound == TTBound::UpperBound ? -10000 : 9000;
                        const EvalValue beta  = alpha + 1000;
                        tt.clear();
                        load(Board(fen(clocks.current)));
                        const auto cold = search(pv, depth, alpha, beta);
                        tt.clear();
                        const Board source(fen(clocks.stored));
                        tt.store(source.tt_key(), NULL_MOVE, score, depth, bound, 0);
                        load(Board(fen(clocks.current)));
                        const auto actual = search(pv, depth, alpha, beta);
                        EXPECT_EQ(actual, clocks.share ? score : cold);
                        if (clocks.share)
                            EXPECT_EQ(worker.node_count(), 1U);
                        else
                            EXPECT_GT(worker.node_count(), 1U);
                    }
                }
            }
        }
    }
}

TEST_F(TtClockTest, ClockGroupsSeparateUpperBoundPruningVetoes) {
    // The null child supplies a fail-high score; only a compatible parent upper
    // bound can veto null-move pruning.
    const auto fen = [](int clock) {
        return "7k/8/8/8/8/8/8/K2Q4 w - - " + std::to_string(clock) + " 1";
    };
    for (int stored_clock : {0, 16, 80, 98}) {
        tt.clear();
        const Board target(fen(0));
        Board       child(target);
        child.make_null();
        tt.store(child.tt_key(), NULL_MOVE, -200, 1, TTBound::Exact, 1);
        tt.store(Board(fen(stored_clock)).tt_key(), NULL_MOVE, 49, 4, TTBound::UpperBound, 0);
        load(target);
        const auto value = search(false, 4, -50, 50);
        if (stored_clock == 0)
            EXPECT_GT(worker.node_count(), 2U);
        else {
            EXPECT_EQ(value, 200);
            EXPECT_EQ(worker.node_count(), 2U);
        }
    }
}

} // namespace
} // namespace search
