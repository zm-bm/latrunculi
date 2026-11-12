#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "core/constants.hpp"
#include "eval/evaluation.hpp"
#include "movegen/moves.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

namespace search {

namespace {

using NodeType = SearchTestAccess::NodeType;

class QuiescenceTest : public ::testing::Test {
protected:
    RecordingSearchReporter reporter;
    ThreadPool pool{1, reporter};
    Worker& worker{SearchThreadTestAccess::worker(pool)};
    Limits limits;
    int current_ply{0};

    void SetUp() override {
        limits.depth = 4;
        tt.clear();
    }

    void load(const Board& board) {
        tt.clear();
        worker.configure_search(board, limits, SearchClock::now());
        SearchTestAccess::reset(worker);
        current_ply = 0;
    }

    Board& position() { return SearchTestAccess::board(worker); }
    int& ply() { return current_ply; }

    Score search(Score alpha, Score beta) {
        return SearchTestAccess::quiescence<NodeType::NonPv>(worker, alpha, beta, nullptr, ply());
    }

    Score pv_search(Score alpha, Score beta, PV& pv) {
        return SearchTestAccess::quiescence<NodeType::Pv>(worker, alpha, beta, &pv, ply());
    }

    Score depth_zero(Score alpha, Score beta) {
        return SearchTestAccess::alphabeta<NodeType::NonPv>(worker, alpha, beta, 0, nullptr, ply());
    }

    Move find_move(std::string_view move_string) {
        return movegen::UnscoredMoves(position()).next_matching([&](Move move) {
            return move.str() == move_string;
        });
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
        return tt.probe(SearchTestAccess::board(worker).tt_key()).record;
    }

#if LATRUNCULI_SEARCH_STATS
    const Counters& counters() { return SearchTestAccess::instrumentation(worker).raw_counters(); }
#endif
};

} // namespace

TEST_F(QuiescenceTest, DepthZeroDispatchesToQuiescence) {
    Board board{"k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1"};
    load(board);

    const Score static_eval = eval::evaluate(position());
    EXPECT_GT(depth_zero(-scores::inf, scores::inf), static_eval);
    EXPECT_GT(worker.node_count(), 1U);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().qnodes[0], 0U);
#endif
}

TEST_F(QuiescenceTest, TerminatesDrawAndMaxPlyWithoutTtStore) {
    constexpr auto drawn = board_test::fen::kings_only;

    Board depth_zero_draw{drawn};
    load(depth_zero_draw);
    EXPECT_EQ(depth_zero(-scores::inf, scores::inf), scores::draw);
    EXPECT_EQ(worker.node_count(), 1U);
    EXPECT_FALSE(record().has_value());

    Board qsearch_draw{drawn};
    load(qsearch_draw);
    EXPECT_EQ(search(-scores::inf, scores::inf), scores::draw);
    EXPECT_EQ(worker.node_count(), 1U);
    EXPECT_FALSE(record().has_value());

    Board max_ply{board_test::fen::quiet_black_to_move};
    load(max_ply);
    ply() = engine::max_search_ply;
    auto& terminal = SearchTestAccess::frames(worker)[ply()];
    terminal.pv.update(Move{E2, E4}, PV{});
    PV pv;
    EXPECT_EQ(pv_search(-scores::inf, scores::inf, pv), eval::evaluate(position()));
    EXPECT_TRUE(terminal.state.pv_enabled);
    EXPECT_TRUE(terminal.pv.empty());
    EXPECT_TRUE(pv.empty());
    EXPECT_EQ(worker.node_count(), 1U);
    EXPECT_FALSE(record().has_value());
}

TEST_F(QuiescenceTest, AdjudicatesFiftyMoveBoundaryBeforeTtAndMaxPly) {
    struct Case {
        const char* fen;
        int search_ply;
        Score value;
    };
    constexpr std::array cases{
        Case{"7k/6Q1/5K2/8/8/8/8/8 b - - 100 1", 3, -scores::mate + 3},
        Case{"7k/6Q1/5K2/8/8/8/8/8 b - - 100 1",
             engine::max_search_ply,
             -scores::mate + engine::max_search_ply},
        Case{"7k/7Q/5K2/8/8/8/8/8 b - - 100 1", 3, scores::draw},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.fen);
        SCOPED_TRACE(tc.search_ply);
        Board board{tc.fen};
        load(board);
        ply() = tc.search_ply;
        tt.store(position().tt_key(), NULL_MOVE, 1234, 8, TTBound::Exact, ply());
        EXPECT_EQ(search(-1, 0), tc.value);
        PV pv;
        EXPECT_EQ(pv_search(-scores::inf, scores::inf, pv), tc.value);
    }
}

TEST_F(QuiescenceTest, StandPatFailHighStoresLowerBound) {
    Board board{board_test::fen::quiet_black_to_move};
    load(board);

    const Score static_eval = eval::evaluate(position());
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
    const Score expected = -with_move(evasion, [&] { return search(-scores::inf, scores::inf); });

    load(evasion_board);
    EXPECT_EQ(search(-scores::inf, scores::inf), expected);

    Board mate{"7k/6Q1/6K1/8/8/8/8/8 b - - 0 1"};
    load(mate);
    ply() = 3;
    EXPECT_EQ(search(-scores::inf, scores::inf), -scores::mate + 3);

    const auto stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->score, -scores::mate);
    EXPECT_EQ(stored->bound, TTBound::Exact);
    EXPECT_EQ(stored->move, NULL_MOVE);
}

TEST_F(QuiescenceTest, PvSearchRebuildsContinuationWithRetainedTt) {
    Board board{"k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1"};
    load(board);
    const Move capture = find_move("d1e2");
    ASSERT_FALSE(capture.is_null());

    PV cold_pv;
    const Score value = pv_search(-scores::inf, scores::inf, cold_pv);
    EXPECT_GT(value, eval::evaluate(position()));
    ASSERT_EQ(cold_pv.size(), 1);

    const auto stored = record();
    ASSERT_TRUE(stored.has_value());
    ASSERT_EQ(stored->bound, TTBound::Exact);

    PV warm_pv;
    EXPECT_EQ(pv_search(-scores::inf, scores::inf, warm_pv), value);
    ASSERT_EQ(warm_pv.size(), 1);
    EXPECT_EQ(warm_pv.front(), capture);
}

TEST_F(QuiescenceTest, SettlesTacticalPositionWithoutSearchState) {
    constexpr auto tactical = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    Board first{tactical};
    Board intervening{board_test::fen::one_legal_evasion};
    Board second{tactical};

    tt.store(first.tt_key(), NULL_MOVE, -scores::mate, 0, TTBound::Exact, 0);
    ASSERT_TRUE(pool.settle(first));
    tt.clear();
    ASSERT_TRUE(pool.settle(intervening));
    ASSERT_TRUE(pool.settle(second));

    EXPECT_EQ(first.to_fen(), second.to_fen());
    EXPECT_EQ(first.to_fen(), "k7/8/8/8/8/8/4Q3/K7 b - - 0 1");
}

TEST_F(QuiescenceTest, SettlingHandlesCheckEvasion) {
    Board checked{board_test::fen::one_legal_evasion};
    ASSERT_TRUE(checked.is_check());

    ASSERT_TRUE(pool.settle(checked));

    EXPECT_FALSE(checked.is_check());
    EXPECT_EQ(checked.to_fen(), "1k6/8/2K5/8/8/8/R7/8 w - - 1 2");
}

TEST_F(QuiescenceTest, SettlingKeepsQuietPositionAndRejectsTerminalPositions) {
    Board quiet{board_test::fen::start};
    const std::string original = quiet.to_fen();
    ASSERT_TRUE(pool.settle(quiet));
    EXPECT_EQ(quiet.to_fen(), original);

    Board mate{"7k/6Q1/6K1/8/8/8/8/8 b - - 0 1"};
    const std::string checkmate = mate.to_fen();
    EXPECT_FALSE(pool.settle(mate));
    EXPECT_EQ(mate.to_fen(), checkmate);

    Board stalemate{"7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"};
    const std::string drawn = stalemate.to_fen();
    EXPECT_FALSE(pool.settle(stalemate));
    EXPECT_EQ(stalemate.to_fen(), drawn);
}

TEST_F(QuiescenceTest, UsesEligibleTtCutoffs) {
    struct Case {
        const char* name;
        TTBound bound;
        Score score;
        Score alpha;
        Score beta;
    };

    constexpr std::array cases{
        Case{"exact", TTBound::Exact, 321, -scores::inf, scores::inf},
        Case{"lower", TTBound::LowerBound, 500, 100, 200},
        Case{"upper", TTBound::UpperBound, -500, -200, -100},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{board_test::fen::quiet_black_to_move};
        load(board);
        tt.store(position().tt_key(), NULL_MOVE, tc.score, 0, tc.bound, ply());
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
    const Score baseline = search(-scores::inf, scores::inf);

    Board board{board_test::fen::perft_position_3};
    load(board);
    const Move quiet{E2, E3};
    ASSERT_TRUE(position().is_pseudo_legal(quiet));
    ASSERT_FALSE(position().is_capture(quiet));
    tt.store(position().tt_key(), quiet, -scores::inf + 1000, 0, TTBound::LowerBound, ply());

    EXPECT_EQ(search(-scores::inf, scores::inf), baseline);
}

TEST_F(QuiescenceTest, StoresWindowClassifiedTtBounds) {
    constexpr auto tactical = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";

    Board exact_source{tactical};
    load(exact_source);
    const Score static_eval = eval::evaluate(position());
    const Score exact = search(-scores::inf, scores::inf);
    ASSERT_GT(exact, static_eval);

    Board lower_board{tactical};
    load(lower_board);
    const Score beta = static_eval + std::max(1, (exact - static_eval) / 2);
    EXPECT_GE(search(beta - 100, beta), beta);
    auto stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->bound, TTBound::LowerBound);
    EXPECT_FALSE(stored->move.is_null());

    Board upper_board{board_test::fen::quiet_black_to_move};
    load(upper_board);
    const Score upper_eval = eval::evaluate(position());
    EXPECT_LT(search(upper_eval + 1, upper_eval + 100), upper_eval + 1);
    stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->bound, TTBound::UpperBound);
    EXPECT_EQ(stored->move, NULL_MOVE);

    Board exact_board{tactical};
    load(exact_board);
    EXPECT_GT(search(static_eval - 1, scores::inf), static_eval);
    stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->bound, TTBound::Exact);
    EXPECT_FALSE(stored->move.is_null());
}

TEST_F(QuiescenceTest, PvNodeIgnoresLowerTtBound) {
    const Board board{board_test::fen::quiet_black_to_move};
    load(board);
    PV baseline_pv;
    const Score baseline = pv_search(-scores::inf, scores::inf, baseline_pv);

    const Score alpha = baseline - 100;
    const Score beta = baseline + 100;
    const Score bogus = beta + 25;

    load(board);
    tt.store(position().tt_key(), NULL_MOVE, bogus, 0, TTBound::LowerBound, ply());
    PV pv;
    EXPECT_EQ(pv_search(alpha, beta, pv), baseline);
}

TEST_F(QuiescenceTest, PvNodePropagatesToChildTtPolicy) {
    constexpr auto tactical = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    Board baseline_board{tactical};
    load(baseline_board);

    const Score static_eval = eval::evaluate(position());
    const Score alpha = static_eval - 100;
    const Score beta = static_eval + 500;
    PV baseline_pv;
    const Score baseline = pv_search(alpha, beta, baseline_pv);

    Board bounded_board{tactical};
    load(bounded_board);
    const Move capture = find_move("d1e2");
    ASSERT_FALSE(capture.is_null());
    with_move(capture, [&] {
        tt.store(position().tt_key(), NULL_MOVE, -static_eval + 10, 0, TTBound::LowerBound, ply());
        return 0;
    });

    PV pv;
    EXPECT_EQ(pv_search(alpha, beta, pv), baseline);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().q_tt_probes[1], 0U);
    EXPECT_GT(counters().q_tt_hits[1], 0U);
    EXPECT_EQ(counters().q_tt_cutoffs[1], 0U);
#endif
}

TEST_F(QuiescenceTest, CaptureGainPruningReturnsUpperEstimateWithoutPv) {
    load(Board{"1n2k1nr/8/8/3p4/4P3/8/8/RN2K1N1 w - - 0 1"});
    const Score stand_pat = eval::evaluate(position());
    const Score estimate = stand_pat + eval::pawn.eg + 292;

    PV pv;
    EXPECT_EQ(pv_search(estimate + 1, estimate + 2, pv), estimate);
    EXPECT_EQ(worker.node_count(), 1U);
    EXPECT_EQ(pv.size(), 0);
    const auto stored = record();
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->bound, TTBound::UpperBound);
    EXPECT_EQ(stored->score_at_ply(ply()), estimate);
}

TEST_F(QuiescenceTest, CaptureGainUsesEvaluationUnitsForMinorVictim) {
    load(Board{"1n2k1nr/8/8/3n4/4P3/8/8/RN2K1N1 w - - 0 1"});
    const Score alpha = eval::evaluate(position()) + 700;
    // A SEE knight price plus the margin would incorrectly discard this capture.
    search(alpha, alpha + 1);
    EXPECT_GT(worker.node_count(), 1U);
}

TEST_F(QuiescenceTest, CaptureGainPreservesCheckingMateFarBelowAlpha) {
    load(Board{"rn3rk1/5ppp/8/8/8/3B4/8/RN2K1NQ w - - 0 1"});
    const Score alpha = eval::evaluate(position()) + 5000;
    PV pv;
    EXPECT_EQ(pv_search(alpha, alpha + 1, pv), scores::mate - 1);
    ASSERT_GT(pv.size(), 0);
    EXPECT_EQ(pv.front(), Move(H1, H7));
}

} // namespace search
