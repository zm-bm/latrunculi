#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "eval/evaluation.hpp"
#include "movegen/generator.hpp"
#include "search/algorithm_detail.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

namespace search {
namespace {

class SingularTest : public ::testing::Test {
protected:
    static constexpr auto   one_quiet = "k7/8/1PK5/8/8/8/P7/8 b - - 0 1";
    RecordingSearchReporter reporter;
    ThreadPool              pool{1, reporter};
    Worker&                 worker{SearchThreadTestAccess::worker(pool)};

    void load(const Board& b, NodeCount budget = 0) {
        tt.clear();
        Limits limits;
        limits.depth = 12;
        if (budget)
            limits.set_nodes(budget);
        worker.configure_search(b, limits, SearchClock::now());
        SearchTestAccess::reset(worker);
        SearchTestAccess::ordering_state(worker).clear();
        ply()                                          = 1;
        SearchTestAccess::root_iteration_depth(worker) = 12;
    }

    Board& board() { return SearchTestAccess::board(worker); }
    int&   ply() { return SearchTestAccess::search_ply(worker); }
    Move   move(std::string_view name) {
        for (Move m : movegen::generate_pseudo_legal(board()))
            if (m.str() == name && board().is_legal_pseudo_move(m))
                return m;
        return NULL_MOVE;
    }
    TTRecord entry(Move m, int score = 1000, int depth = 5, TTBound bound = TTBound::LowerBound) {
        return TTRecord{
            .move = m, .score = std::int16_t(score), .depth = std::uint8_t(depth), .bound = bound};
    }
    void seed_alternative(EvalValue value) {
        const Move alternative = move("a8a7");
        ASSERT_FALSE(alternative.is_null());
        board().make(alternative);
        tt.store(board().tt_key(),
                 NULL_MOVE,
                 -value,
                 engine::max_search_depth,
                 TTBound::Exact,
                 ply() + 1);
        board().unmake();
    }
    template <NodeType Node = NodeType::Pv>
    int
    extend(const TTRecord& record, int depth = 8, bool can_null = true, Move excluded = NULL_MOVE) {
        return SearchTestAccess::singular_extension<Node>(
            worker, record, depth, can_null, excluded);
    }
    EvalValue excluded_search(Move excluded, int depth, EvalValue alpha, EvalValue beta) {
        return SearchTestAccess::alphabeta<NodeType::NonPv>(
            worker, alpha, beta, depth, nullptr, true, excluded);
    }
};

} // namespace

TEST_F(SingularTest, ExcludingTheOnlyLegalMoveDoesNotCreateMateOrStalemate) {
    for (const char* fen : {one_quiet, board_test::fen::one_legal_evasion}) {
        SCOPED_TRACE(fen);
        load(Board{fen});
        const auto original = board().to_fen();
        const Move only     = move("a8b8");
        ASSERT_FALSE(only.is_null());
        tt.store(board().tt_key(), only, 1000, 9, TTBound::Exact, ply());
        EXPECT_EQ(excluded_search(only, 3, 200, 201), 200);
        EXPECT_EQ(board().to_fen(), original);
        const auto stored = tt.probe(board().tt_key()).record;
        ASSERT_TRUE(stored);
        EXPECT_EQ(stored->score_at_ply(ply()), 1000);
        EXPECT_EQ(stored->depth, 9);
        EXPECT_EQ(stored->bound, TTBound::Exact);
        EXPECT_EQ(stored->move, only);
    }
}

TEST_F(SingularTest, VerificationFailHighCannotStoreOrdinaryBoundsOrTrainRefutations) {
    load(Board{board_test::fen::quiet_black_to_move});
    const Move excluded    = move("a8b8");
    const Move alternative = move("a8a7");
    ASSERT_FALSE(excluded.is_null());
    seed_alternative(900);
    tt.store(board().tt_key(), excluded, -500, 9, TTBound::Exact, ply());
    EXPECT_EQ(excluded_search(excluded, 3, 499, 500), 900);
    const auto stored = tt.probe(board().tt_key()).record;
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->score_at_ply(ply()), -500);
    EXPECT_EQ(stored->bound, TTBound::Exact);
    EXPECT_EQ(stored->move, excluded);
    const auto& history = SearchTestAccess::ordering_state(worker);
    EXPECT_EQ(history.quiets.get(BLACK, A8, A7), 0);
    EXPECT_FALSE(history.is_killer(alternative, ply()));
}

TEST_F(SingularTest, VerifierChildrenRetainOrdinaryTtBehavior) {
    load(Board{board_test::fen::quiet_black_to_move});
    const Move excluded    = move("a8b8");
    const Move alternative = move("a8a7");
    excluded_search(excluded, 1, -eval_value::inf, eval_value::inf);
    EXPECT_FALSE(tt.probe(board().tt_key()).record);
    board().make(alternative);
    EXPECT_TRUE(tt.probe(board().tt_key()).record);
    board().unmake();
}

TEST_F(SingularTest, EqualityRefutesSingularityWhileFailLowExtends) {
    for (bool fail_low : {false, true}) {
        SCOPED_TRACE(fail_low);
        load(Board{board_test::fen::quiet_black_to_move});
        const auto      record    = entry(move("a8b8"));
        const EvalValue threshold = record.score - algorithm_detail::singular_margin(8);
        seed_alternative(threshold - int(fail_low));
        EXPECT_EQ(extend(record), int(fail_low));
        EXPECT_GT(worker.node_count(), 1U);
    }
}

TEST_F(SingularTest, VerificationRestoresParentEvaluationState) {
    load(Board{board_test::fen::quiet_black_to_move});
    const auto record = entry(move("a8b8"));
    seed_alternative(-200);
    tt.store(board().tt_key(), record.move, record.score, record.depth, record.bound, ply());
    SearchTestAccess::static_evals(worker)[ply()] = 123;
    SearchTestAccess::real_moves(worker)[ply()]   = true;
    const auto original                           = board().to_fen();
    EXPECT_EQ(extend(record), 1);
    EXPECT_EQ(SearchTestAccess::static_evals(worker)[ply()], 123);
    EXPECT_TRUE(SearchTestAccess::real_moves(worker)[ply()]);
    EXPECT_EQ(board().to_fen(), original);
    EXPECT_EQ(tt.probe(board().tt_key()).record->score_at_ply(ply()), record.score);
}

TEST_F(SingularTest, StopsDuringVerificationWithoutExtendingOrLeakingState) {
    load(Board{board_test::fen::quiet_black_to_move}, 4096);
    const auto record                             = entry(move("a8b8"));
    SearchTestAccess::static_evals(worker)[ply()] = 123;
    SearchTestAccess::real_moves(worker)[ply()]   = true;
    SearchTestAccess::set_nodes(worker, 4095);
    const auto original = board().to_fen();
    EXPECT_EQ(extend(record), 0);
    EXPECT_GE(worker.node_count(), 4096U);
    EXPECT_EQ(SearchTestAccess::static_evals(worker)[ply()], 123);
    EXPECT_TRUE(SearchTestAccess::real_moves(worker)[ply()]);
    EXPECT_EQ(board().to_fen(), original);
    EXPECT_FALSE(tt.probe(board().tt_key()).record);
}

TEST_F(SingularTest, RequiresDepthReliableLegalNonMateTtEvidence) {
    load(Board{one_quiet});
    auto record = entry(move("a8b8"));
    EXPECT_EQ(extend(record, 7), 0);
    record.depth = 4;
    EXPECT_EQ(extend(record), 0);
    record.depth = 5;
    record.bound = TTBound::UpperBound;
    EXPECT_EQ(extend(record), 0);
    record.bound = TTBound::LowerBound;
    record.score = eval_value::mate - 1;
    EXPECT_EQ(extend(record), 0);
    record.score = 1000;
    record.move  = NULL_MOVE;
    EXPECT_EQ(extend(record), 0);
    EXPECT_EQ(worker.node_count(), 0U);
    record.move = move("a8b8");
    EXPECT_EQ(extend(record), 1);

    load(Board{"k3r3/8/8/8/8/8/P3R3/4K3 w - - 0 1"});
    const Move pinned{E2, D2};
    ASSERT_TRUE(board().is_pseudo_legal(pinned));
    ASSERT_FALSE(board().is_legal_pseudo_move(pinned));
    EXPECT_EQ(extend(entry(pinned)), 0);
    EXPECT_EQ(worker.node_count(), 0U);
}

TEST_F(SingularTest, RespectsRootNullExclusionAndPathLimits) {
    load(Board{one_quiet});
    const auto record = entry(move("a8b8"));
    EXPECT_EQ(extend(record, 8, false), 0);
    EXPECT_EQ(extend(record, 8, true, record.move), 0);
    ply() = 0;
    EXPECT_EQ(extend(record), 0);
    ply() = 24;
    EXPECT_EQ(extend(record), 0);
    ply() = engine::max_search_ply - 1;
    EXPECT_EQ(extend(record), 0);
    EXPECT_EQ(worker.node_count(), 0U);
    SearchTestAccess::reset(worker);
    EXPECT_EQ(SearchTestAccess::root_iteration_depth(worker), 0);
}

TEST_F(SingularTest, AppliesTheVariantNodeTypePolicy) {
    load(Board{one_quiet});
    auto record = entry(move("a8b8"));
    EXPECT_EQ(extend<NodeType::NonPv>(record), 1);
    load(Board{one_quiet});
    record = entry(move("a8b8"));
    EXPECT_EQ(extend<NodeType::Pv>(record), 1);
}

TEST_F(SingularTest, ExtendsTheActualChildAndRebuildsPvWithRetainedTt) {
    load(Board{board_test::fen::one_legal_evasion});
    const Move only = move("a8b8");
    tt.store(board().tt_key(), only, 30000, 7, TTBound::LowerBound, ply());
    PrincipalVariation pv;
    SearchTestAccess::alphabeta<NodeType::Pv>(worker, -eval_value::inf, eval_value::inf, 8, &pv);
    ASSERT_GT(pv.size(), 0);
    EXPECT_EQ(pv.front(), only);
    board().make(only);
    const auto child = tt.probe(board().tt_key()).record;
    ASSERT_TRUE(child);
    EXPECT_GE(child->depth, 8);
    board().unmake();
    SearchTestAccess::alphabeta<NodeType::Pv>(worker, -eval_value::inf, eval_value::inf, 8, &pv);
    ASSERT_GT(pv.size(), 0);
    Board copy = board();
    for (int i = 0; i < pv.size(); ++i) {
        ASSERT_TRUE(copy.is_pseudo_legal(pv.move_at(i)));
        ASSERT_TRUE(copy.is_legal_pseudo_move(pv.move_at(i)));
        copy.make(pv.move_at(i));
    }
}

} // namespace search
