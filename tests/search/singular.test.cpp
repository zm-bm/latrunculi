#include <array>

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

    Board&       board() { return SearchTestAccess::board(worker); }
    int&         ply() { return SearchTestAccess::search_ply(worker); }
    SearchFrame& frame() { return SearchTestAccess::frames(worker)[ply()]; }
    Move         move(std::string_view name) {
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
    bool extend(const TTRecord& record, int depth = 8, bool can_null = true) {
        return SearchTestAccess::singular_result<Node>(
                   worker, record, depth, -eval_value::inf, eval_value::inf, can_null)
            .extend;
    }
    EvalValue excluded_search(Move excluded, int depth, EvalValue alpha, EvalValue beta) {
        return SearchTestAccess::search_excluding_move(worker, alpha, beta, depth, excluded);
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
    frame().static_eval    = 123;
    frame().real_move      = true;
    const auto saved_frame = frame();
    const auto original    = board().to_fen();
    EXPECT_EQ(extend(record), 1);
    EXPECT_EQ(frame(), saved_frame);
    EXPECT_EQ(board().to_fen(), original);
    EXPECT_EQ(tt.probe(board().tt_key()).record->score_at_ply(ply()), record.score);
}

TEST_F(SingularTest, StopsDuringVerificationWithoutExtendingOrLeakingState) {
    load(Board{board_test::fen::quiet_black_to_move}, 4096);
    const auto record      = entry(move("a8b8"));
    frame().static_eval    = 123;
    frame().real_move      = true;
    const auto saved_frame = frame();
    SearchTestAccess::set_nodes(worker, 4095);
    const auto original = board().to_fen();
    EXPECT_EQ(extend(record), 0);
    EXPECT_GE(worker.node_count(), 4096U);
    EXPECT_EQ(frame(), saved_frame);
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
    frame().excluded_move = record.move;
    EXPECT_EQ(extend(record), 0);
    frame().excluded_move = NULL_MOVE;
    ply()                 = 0;
    EXPECT_EQ(extend(record), 0);
    ply() = 24;
    EXPECT_EQ(extend(record), 0);
    ply() = engine::max_search_ply - 1;
    EXPECT_EQ(extend(record), 0);
    EXPECT_EQ(worker.node_count(), 0U);
    SearchTestAccess::reset(worker);
    EXPECT_EQ(SearchTestAccess::root_iteration_depth(worker), 0);
}

TEST_F(SingularTest, ExtendsSingularMovesAtPvAndNonPvNodes) {
    for (bool verification : {false, true}) {
        SCOPED_TRACE(verification);
        load(Board{one_quiet});
        frame().in_singular_verification                                     = verification;
        SearchTestAccess::frames(worker)[ply() - 1].in_singular_verification = verification;
        const auto record                                                    = entry(move("a8b8"));
        const auto saved_frame                                               = frame();
        EXPECT_TRUE(extend<NodeType::NonPv>(record));
        EXPECT_EQ(frame(), saved_frame);
        EXPECT_TRUE(extend<NodeType::Pv>(record));
        EXPECT_EQ(frame(), saved_frame);
    }
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

TEST_F(SingularTest, RefutationEqualityAllowsMulticutInsteadOfExtension) {
    for (int offset : {-1, 0, 1}) {
        load(Board{board_test::fen::quiet_black_to_move});
        const auto record = entry(move("a8b8"));
        const auto sb     = record.score - algorithm_detail::singular_margin(8);
        seed_alternative(sb + offset);
        const auto result = SearchTestAccess::singular_result(worker, record, 8, sb - 1, sb);
        EXPECT_EQ(result.extend, offset < 0);
        EXPECT_EQ(result.cutoff.has_value(), offset >= 0);
        if (result.cutoff)
            EXPECT_EQ(*result.cutoff, sb);
        EXPECT_FALSE(frame().in_singular_verification);
    }
}

TEST_F(SingularTest, MulticutRequiresThresholdAtOrAboveBeta) {
    for (int beta_offset : {-1, 0, 1}) {
        SCOPED_TRACE(beta_offset);
        load(Board{board_test::fen::quiet_black_to_move});
        const auto record = entry(move("a8b8"));
        const auto sb     = record.score - algorithm_detail::singular_margin(8);
        seed_alternative(sb);
        const auto beta   = sb + beta_offset;
        const auto result = SearchTestAccess::singular_result(worker, record, 8, beta - 1, beta);
        EXPECT_FALSE(result.extend);
        EXPECT_EQ(result.cutoff.has_value(), beta_offset <= 0);
        if (result.cutoff)
            EXPECT_EQ(*result.cutoff, sb);
        EXPECT_GT(worker.node_count(), 1U);
    }
}

TEST_F(SingularTest, MulticutReturnsThresholdWithoutOrdinaryTtOrHistoryTraining) {
    load(Board{board_test::fen::quiet_black_to_move});
    const auto record = entry(move("a8b8"));
    const auto sb     = record.score - algorithm_detail::singular_margin(8);
    seed_alternative(sb + 150);
    tt.store(board().tt_key(), record.move, record.score, record.depth, record.bound, ply());
    frame().static_eval = 321;
    frame().real_move   = true;
    const auto original = board().to_fen();
    EXPECT_EQ(SearchTestAccess::alphabeta<NodeType::NonPv>(worker, sb - 1, sb, 8), sb);
    EXPECT_EQ(board().to_fen(), original);
    const auto stored = tt.probe(board().tt_key()).record;
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->depth, record.depth);
    EXPECT_EQ(stored->score, record.score);
    EXPECT_EQ(stored->move, record.move);
    const auto& history = SearchTestAccess::ordering_state(worker);
    EXPECT_EQ(history.quiets.get(BLACK, A8, A7), 0);
    EXPECT_EQ(history.quiets.get(BLACK, A8, B8), 0);
    EXPECT_FALSE(history.is_killer(move("a8a7"), ply()));
    EXPECT_FALSE(history.is_killer(record.move, ply()));
    // alphabeta established the parent evaluation before verification, which must survive it.
    EXPECT_TRUE(frame().static_eval.has_value());
    EXPECT_FALSE(frame().in_singular_verification);
}

TEST_F(SingularTest, RefutedSingularityKeepsOriginalChildDepth) {
    for (bool verification : {false, true}) {
        SCOPED_TRACE(verification);
        load(Board{board_test::fen::quiet_black_to_move});
        // A reused slot must obtain context from its current parent.
        frame().in_singular_verification                                     = !verification;
        SearchTestAccess::frames(worker)[ply() - 1].in_singular_verification = verification;
        const auto record                                                    = entry(move("a8b8"));
        const auto sb = record.score - algorithm_detail::singular_margin(8);
        seed_alternative(sb);
        tt.store(board().tt_key(), record.move, record.score, record.depth, record.bound, ply());
        const auto original = board().to_fen();
        SearchTestAccess::alphabeta<NodeType::NonPv>(worker, record.score - 1, record.score, 8);
        EXPECT_EQ(board().to_fen(), original);
        EXPECT_EQ(frame().in_singular_verification, verification);
        board().make(record.move);
        const auto child = tt.probe(board().tt_key()).record;
        ASSERT_TRUE(child);
        EXPECT_EQ(child->depth, 7);
        board().unmake();
    }
}

TEST_F(SingularTest, MulticutProtectsPvChecksAndMateWindows) {
    for (auto window :
         std::array<std::pair<EvalValue, EvalValue>, 4>{{{-eval_value::inf, 100},
                                                         {-eval_value::mate_bound, 100},
                                                         {100, eval_value::mate_bound},
                                                         {100, eval_value::inf}}}) {
        load(Board{board_test::fen::quiet_black_to_move});
        const auto record = entry(move("a8b8"));
        const auto sb     = record.score - algorithm_detail::singular_margin(8);
        seed_alternative(sb);
        const auto r =
            SearchTestAccess::singular_result(worker, record, 8, window.first, window.second);
        EXPECT_FALSE(r.extend);
        EXPECT_FALSE(r.cutoff);
    }
    load(Board{board_test::fen::quiet_black_to_move});
    auto record = entry(move("a8b8"));
    auto sb     = record.score - algorithm_detail::singular_margin(8);
    seed_alternative(sb);
    const auto pv = SearchTestAccess::singular_result<NodeType::Pv>(worker, record, 8, sb - 1, sb);
    EXPECT_FALSE(pv.extend);
    EXPECT_FALSE(pv.cutoff);

    load(Board{"k7/8/3K4/8/8/8/R7/8 b - - 0 1"});
    ASSERT_TRUE(board().is_check());
    record = entry(move("a8b8"));
    sb     = record.score - algorithm_detail::singular_margin(8);
    for (Move m : movegen::generate_evasions(board())) {
        if (m == record.move || !board().is_legal_pseudo_move(m))
            continue;
        board().make(m);
        tt.store(
            board().tt_key(), NULL_MOVE, -sb, engine::max_search_depth, TTBound::Exact, ply() + 1);
        board().unmake();
    }
    const auto checked = SearchTestAccess::singular_result(worker, record, 8, sb - 1, sb);
    EXPECT_FALSE(checked.extend);
    EXPECT_FALSE(checked.cutoff);
}

TEST_F(SingularTest, VerificationWindowMustAlsoStayStrictlyOutsideMateRanges) {
    load(Board{board_test::fen::quiet_black_to_move});
    auto       record = entry(move("a8b8"),
                        -eval_value::mate_bound + 1 - ply() + algorithm_detail::singular_margin(8));
    const auto sb     = record.score_at_ply(ply()) - algorithm_detail::singular_margin(8);
    ASSERT_EQ(sb - 1, -eval_value::mate_bound);
    seed_alternative(sb);
    const auto r = SearchTestAccess::singular_result(worker, record, 8, sb, sb + 1);
    EXPECT_FALSE(r.extend);
    EXPECT_FALSE(r.cutoff);
}

TEST_F(SingularTest, VerificationContextRestoresNestedAndInterruptedScopesAndResets) {
    for (bool outer : {false, true}) {
        load(Board{board_test::fen::quiet_black_to_move});
        auto record = entry(move("a8b8"));
        auto sb     = record.score - algorithm_detail::singular_margin(8);
        seed_alternative(sb);
        frame().in_singular_verification                                     = outer;
        SearchTestAccess::frames(worker)[ply() - 1].in_singular_verification = outer;
        frame().static_eval                                                  = 123;
        frame().real_move                                                    = true;
        const auto saved_frame                                               = frame();
        const auto r = SearchTestAccess::singular_result(worker, record, 8, sb - 1, sb);
        EXPECT_EQ(frame(), saved_frame);
        if (outer) {
            EXPECT_FALSE(r.extend);
            EXPECT_FALSE(r.cutoff);
        }

        load(Board{board_test::fen::quiet_black_to_move}, 4096);
        frame().in_singular_verification                                     = outer;
        SearchTestAccess::frames(worker)[ply() - 1].in_singular_verification = outer;
        record                                                               = entry(move("a8b8"));
        sb                           = record.score - algorithm_detail::singular_margin(8);
        frame().static_eval          = 456;
        frame().real_move            = true;
        const auto interrupted_frame = frame();
        SearchTestAccess::set_nodes(worker, 4095);
        const auto stopped =
            SearchTestAccess::singular_result<NodeType::Pv>(worker, record, 8, sb - 1, sb);
        EXPECT_FALSE(stopped.extend);
        EXPECT_FALSE(stopped.cutoff);
        EXPECT_GE(worker.node_count(), 4096U);
        EXPECT_EQ(frame(), interrupted_frame);
        SearchTestAccess::reset(worker);
        EXPECT_FALSE(frame().in_singular_verification);
    }
}

TEST_F(SingularTest, ExclusionStaysLocalAndChildContextDoesNotLeakToOrdinarySearches) {
    load(Board{board_test::fen::quiet_black_to_move});
    const Move excluded    = move("a8b8");
    const Move alternative = move("a8a7");
    seed_alternative(900);
    frame().static_eval    = 123;
    frame().real_move      = true;
    const auto saved_frame = frame();
    EXPECT_EQ(excluded_search(excluded, 3, 499, 500), 900);
    EXPECT_EQ(frame(), saved_frame);
    const auto& child = SearchTestAccess::frames(worker)[ply() + 1];
    EXPECT_TRUE(child.excluded_move.is_null());
    EXPECT_TRUE(child.in_singular_verification);

    // Reuse that child slot outside verification, including its ordinary TT cutoff.
    board().make(alternative);
    ++ply();
    EXPECT_EQ(SearchTestAccess::alphabeta<NodeType::NonPv>(worker, -901, -899, 2), -900);
    EXPECT_TRUE(frame().excluded_move.is_null());
    EXPECT_FALSE(frame().in_singular_verification);
    --ply();
    board().unmake();
    EXPECT_EQ(frame(), saved_frame);
}

TEST_F(SingularTest, AlreadyStoppedVerificationRestoresTheCompleteFrame) {
    load(Board{board_test::fen::quiet_black_to_move});
    frame().static_eval              = 123;
    frame().real_move                = true;
    frame().in_singular_verification = true;
    const auto saved_frame           = frame();
    const Move excluded              = move("a8b8");
    worker.request_stop();
    EXPECT_EQ(excluded_search(excluded, 3, 100, 101), 100);
    EXPECT_EQ(frame(), saved_frame);
    EXPECT_EQ(worker.node_count(), 0U);
}

TEST_F(SingularTest, EarlyExitsDoNotAccessOrOverwriteFrames) {
    load(Board{board_test::fen::quiet_black_to_move});
    frame().static_eval              = 123;
    frame().real_move                = true;
    frame().in_singular_verification = true;
    const auto saved_frames          = SearchTestAccess::frames(worker);
    ply()                            = engine::max_search_ply;
    EXPECT_EQ(SearchTestAccess::alphabeta<NodeType::NonPv>(worker, -1000, 1000, 1),
              eval::evaluate(board()));
    EXPECT_EQ(SearchTestAccess::frames(worker), saved_frames);
    worker.request_stop();
    EXPECT_EQ(SearchTestAccess::alphabeta<NodeType::NonPv>(worker, -1000, 1000, 1), -1000);
    EXPECT_EQ(SearchTestAccess::frames(worker), saved_frames);

    load(Board{"7k/8/8/8/8/8/8/K7 w - - 0 1"});
    ASSERT_TRUE(board().is_draw(ply()));
    frame().static_eval     = 456;
    const auto drawn_frames = SearchTestAccess::frames(worker);
    EXPECT_EQ(SearchTestAccess::alphabeta<NodeType::NonPv>(worker, -1000, 1000, 1),
              eval_value::draw);
    EXPECT_EQ(SearchTestAccess::frames(worker), drawn_frames);
}

} // namespace search
