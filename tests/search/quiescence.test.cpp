#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "core/constants.hpp"
#include "eval/evaluator.hpp"
#include "movegen/generator.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

namespace search {

namespace {

class QuiescenceTest : public ::testing::Test {
protected:
    RecordingSearchReporter reporter;
    ThreadPool              pool{1, reporter};
    Worker&                 worker{SearchThreadTestAccess::worker(pool)};
    Limits                  limits;

    void SetUp() override {
        limits.depth = 4;
        tt.clear();
    }

    void load(const Board& board) {
        tt.clear();
        worker.configure_search(board, limits, SearchClock::now());
        SearchTestAccess::reset(worker);
    }

    Board& position() { return SearchTestAccess::board(worker); }
    int&   ply() { return SearchTestAccess::search_ply(worker); }

    EvalValue search(EvalValue alpha, EvalValue beta) {
        return SearchTestAccess::quiescence<NodeType::NonPv>(worker, alpha, beta);
    }

    EvalValue pv_search(EvalValue alpha, EvalValue beta, PrincipalVariation& pv) {
        return SearchTestAccess::quiescence<NodeType::Pv>(worker, alpha, beta, &pv);
    }

    EvalValue depth_zero(EvalValue alpha, EvalValue beta) {
        return SearchTestAccess::alphabeta<NodeType::NonPv>(worker, alpha, beta, 0);
    }

    Move find_move(std::string_view move_string) {
        for (Move move : movegen::generate_pseudo_legal(position())) {
            if (move.str() == move_string && position().is_legal_pseudo_move(move))
                return move;
        }
        return NULL_MOVE;
    }

    template <typename Fn>
    auto with_move(Move move, Fn&& fn) {
        position().make(move);
        ++ply();
        auto result = fn();
        position().unmake();
        --ply();
        return result;
    }

    std::optional<TTRecord> record() const {
        return tt.probe(SearchTestAccess::board(worker).key());
    }

#if LATRUNCULI_SEARCH_STATS
    const Counters& counters() { return SearchTestAccess::instrumentation(worker).raw_counters(); }
#endif
};

} // namespace

TEST_F(QuiescenceTest, DepthZeroDispatchesToQuiescence) {
    Board board{"k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1"};
    load(board);

    const EvalValue static_eval = evaluate(position());
    EXPECT_GT(depth_zero(-eval_value::inf, eval_value::inf), static_eval);
    EXPECT_GT(worker.node_count(), 1U);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().qnodes[0], 0U);
#endif
}

TEST_F(QuiescenceTest, TerminatesDrawAndMaxPlyWithoutTtStore) {
    constexpr auto drawn = "k7/8/8/8/8/8/4r3/K2Q4 w - - 100 1";

    Board depth_zero_draw{drawn};
    load(depth_zero_draw);
    EXPECT_EQ(depth_zero(-eval_value::inf, eval_value::inf), eval_value::draw);
    EXPECT_EQ(worker.node_count(), 1U);
    EXPECT_FALSE(record().has_value());

    Board qsearch_draw{drawn};
    load(qsearch_draw);
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf), eval_value::draw);
    EXPECT_EQ(worker.node_count(), 1U);
    EXPECT_FALSE(record().has_value());

    Board max_ply{board_test::fen::quiet_black_to_move};
    load(max_ply);
    ply() = engine::max_search_ply;
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf), evaluate(position()));
    EXPECT_EQ(worker.node_count(), 1U);
    EXPECT_FALSE(record().has_value());
}

TEST_F(QuiescenceTest, StandPatFailHighStoresLowerBound) {
    Board board{board_test::fen::quiet_black_to_move};
    load(board);

    const EvalValue static_eval = evaluate(position());
    EXPECT_EQ(search(static_eval - 100, static_eval), static_eval);

    const auto stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->score_at_ply(ply()), static_eval);
    EXPECT_EQ(stored->depth, 0);
    EXPECT_EQ(stored->bound, TTBound::LowerBound);
    EXPECT_EQ(stored->move, NULL_MOVE);
}

TEST_F(QuiescenceTest, HandlesCheckEvasionsAndCheckmate) {
    Board evasion_board{board_test::fen::one_legal_evasion};
    load(evasion_board);
    const Move evasion = find_move("a8b8");
    ASSERT_FALSE(evasion.is_null());
    const EvalValue expected =
        -with_move(evasion, [&] { return search(-eval_value::inf, eval_value::inf); });

    load(evasion_board);
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf), expected);

    Board mate{"7k/6Q1/6K1/8/8/8/8/8 b - - 0 1"};
    load(mate);
    ply() = 3;
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf), -eval_value::mate + 3);

    const auto stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->score, -eval_value::mate);
    EXPECT_EQ(stored->bound, TTBound::Exact);
    EXPECT_EQ(stored->move, NULL_MOVE);
}

TEST_F(QuiescenceTest, BuildsPrincipalVariationFromTacticalMove) {
    Board board{"k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1"};
    load(board);

    PrincipalVariation pv;
    EXPECT_GT(pv_search(-eval_value::inf, eval_value::inf, pv), evaluate(position()));
    ASSERT_FALSE(pv.empty());
    EXPECT_TRUE(position().is_legal_move(pv.front()));
}

TEST_F(QuiescenceTest, UsesEligibleTtCutoffs) {
    struct Case {
        const char* name;
        TTBound     bound;
        EvalValue   score;
        EvalValue   alpha;
        EvalValue   beta;
    };

    constexpr std::array cases{
        Case{"exact", TTBound::Exact, 321, -eval_value::inf, eval_value::inf},
        Case{"lower", TTBound::LowerBound, 500, 100, 200},
        Case{"upper", TTBound::UpperBound, -500, -200, -100},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{board_test::fen::quiet_black_to_move};
        load(board);
        tt.store(position().key(), NULL_MOVE, tc.score, 0, tc.bound, ply());
        EXPECT_EQ(search(tc.alpha, tc.beta), tc.score);

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().q_tt_probes[0], 1U);
        EXPECT_EQ(counters().q_tt_hits[0], 1U);
        EXPECT_EQ(counters().q_tt_cutoffs[0], 1U);
#endif
    }
}

TEST_F(QuiescenceTest, IgnoresQuietNoncheckingTtMove) {
    Board baseline_board{board_test::fen::perft_position_3};
    load(baseline_board);
    const EvalValue baseline = search(-eval_value::inf, eval_value::inf);

    Board board{board_test::fen::perft_position_3};
    load(board);
    const Move quiet{E2, E3};
    ASSERT_TRUE(position().is_pseudo_legal(quiet));
    ASSERT_FALSE(position().is_capture(quiet));
    tt.store(position().key(), quiet, -eval_value::inf + 1000, 0, TTBound::LowerBound, ply());

    EXPECT_EQ(search(-eval_value::inf, eval_value::inf), baseline);
}

TEST_F(QuiescenceTest, StoresWindowClassifiedTtBounds) {
    constexpr auto tactical = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";

    Board exact_source{tactical};
    load(exact_source);
    const EvalValue static_eval = evaluate(position());
    const EvalValue exact       = search(-eval_value::inf, eval_value::inf);
    ASSERT_GT(exact, static_eval);

    Board lower_board{tactical};
    load(lower_board);
    const EvalValue beta = static_eval + std::max(1, (exact - static_eval) / 2);
    EXPECT_GE(search(beta - 100, beta), beta);
    auto stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->bound, TTBound::LowerBound);
    EXPECT_FALSE(stored->move.is_null());

    Board upper_board{board_test::fen::quiet_black_to_move};
    load(upper_board);
    const EvalValue upper_eval = evaluate(position());
    EXPECT_LT(search(upper_eval + 1, upper_eval + 100), upper_eval + 1);
    stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->bound, TTBound::UpperBound);
    EXPECT_EQ(stored->move, NULL_MOVE);

    Board exact_board{tactical};
    load(exact_board);
    EXPECT_GT(search(static_eval - 1, eval_value::inf), static_eval);
    stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->bound, TTBound::Exact);
    EXPECT_FALSE(stored->move.is_null());
}

TEST_F(QuiescenceTest, PvNodeIgnoresNonExactTtBound) {
    Board baseline_board{board_test::fen::quiet_black_to_move};
    load(baseline_board);
    PrincipalVariation baseline_pv;
    const EvalValue    baseline = pv_search(-eval_value::inf, eval_value::inf, baseline_pv);

    const EvalValue alpha = baseline - 100;
    const EvalValue beta  = baseline + 100;
    const EvalValue bogus = beta + 25;

    Board non_pv_board{board_test::fen::quiet_black_to_move};
    load(non_pv_board);
    tt.store(position().key(), NULL_MOVE, bogus, 0, TTBound::LowerBound, ply());
    EXPECT_EQ(search(alpha, beta), bogus);

    Board pv_board{board_test::fen::quiet_black_to_move};
    load(pv_board);
    tt.store(position().key(), NULL_MOVE, bogus, 0, TTBound::LowerBound, ply());
    PrincipalVariation pv;
    EXPECT_EQ(pv_search(alpha, beta, pv), baseline);
}

TEST_F(QuiescenceTest, PvNodePropagatesToChildTtPolicy) {
    constexpr auto tactical = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    Board          baseline_board{tactical};
    load(baseline_board);

    const EvalValue    static_eval = evaluate(position());
    const EvalValue    alpha       = static_eval - 100;
    const EvalValue    beta        = static_eval + 500;
    PrincipalVariation baseline_pv;
    const EvalValue    baseline = pv_search(alpha, beta, baseline_pv);

    Board bounded_board{tactical};
    load(bounded_board);
    const Move capture = find_move("d1e2");
    ASSERT_FALSE(capture.is_null());
    with_move(capture, [&] {
        tt.store(position().key(), NULL_MOVE, -static_eval + 10, 0, TTBound::LowerBound, ply());
        return 0;
    });

    PrincipalVariation pv;
    EXPECT_EQ(pv_search(alpha, beta, pv), baseline);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().q_tt_probes[1], 0U);
    EXPECT_GT(counters().q_tt_hits[1], 0U);
    EXPECT_EQ(counters().q_tt_cutoffs[1], 0U);
#endif
}

} // namespace search
