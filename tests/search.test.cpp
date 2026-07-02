#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "board.hpp"
#include "evaluator.hpp"
#include "movegen.hpp"
#include "search_options.hpp"
#include "search_worker.hpp"
#include "test_util.hpp"
#include "thread_test_access.hpp"
#include "threading.hpp"
#include "tt.hpp"
#include "uci.hpp"

namespace {

constexpr auto AnyMove = "ANY";

} // namespace

class SearchTest : public ::testing::Test {
protected:
    uci::Protocol protocol{std::cout, std::cerr};
    ThreadPool    pool{1, protocol};
    Thread*       thread;
    SearchWorker* worker;
    SearchOptions options;

    void SetUp() override {
        thread        = &ThreadTestAccess::thread(pool);
        worker        = &ThreadTestAccess::worker(pool);
        options       = SearchOptions();
        options.depth = 4;
        tt.clear();
    }

    void loadWorkerBoard(Board& board, int search_depth = 4) {
        tt.clear();
        options.board = &board;
        options.depth = search_depth;
        worker->configure_search(options);
        worker->reset_search_state();
    }

    Move findWorkerMove(const std::string& move_str) {
        auto movelist = movegen::generate_pseudo_legal(worker->board);
        auto move_it  = std::find_if(movelist.begin(), movelist.end(), [&](const Move& move) {
            return move.str() == move_str && worker->board.is_legal_generated_move(move);
        });
        EXPECT_NE(move_it, movelist.end()) << worker->board.toFEN();
        return move_it == movelist.end() ? NULL_MOVE : *move_it;
    }

    int runNonPvAlphaBeta(int alpha, int beta, int search_depth) {
        return worker->alphabeta(alpha, beta, search_depth);
    }

    void buildRootLines() { worker->build_root_lines(); }

    int runRootSearch() {
        worker->build_root_lines();
        return worker->search_root();
    }

    bool runRootAspiration(int search_depth, int previous_value) {
        worker->build_root_lines();
        return worker->search_root_aspiration(search_depth, previous_value);
    }

    int runQuiescence(int alpha, int beta) { return worker->quiescence(alpha, beta); }

    int runQuiescenceWithPv(int alpha, int beta, PrincipalVariation& pv) {
        return worker->quiescence(alpha, beta, &pv);
    }

    int runWorkerSearch() { return worker->search(); }

    int testAlphaBetaAfterMove(const std::string& fen,
                               const std::string& move_str,
                               int                alpha,
                               int                beta,
                               int                search_depth) {
        TestBoard board{fen};
        loadWorkerBoard(board, search_depth);

        Move move = findWorkerMove(move_str);
        if (move.is_null())
            return 0;

        worker->board.make(move, worker->position_states.child(worker->ply));
        ++worker->ply;
        int score = worker->alphabeta(alpha, beta, search_depth);
        worker->board.unmake(worker->position_states.parent(worker->ply));
        --worker->ply;
        return score;
    }

    int testQuiescenceAfterMove(const std::string& fen,
                                const std::string& move_str,
                                int                alpha,
                                int                beta) {
        TestBoard board{fen};
        loadWorkerBoard(board);

        Move move = findWorkerMove(move_str);
        if (move.is_null())
            return 0;

        worker->board.make(move, worker->position_states.child(worker->ply));
        ++worker->ply;
        int score = worker->quiescence(alpha, beta);
        worker->board.unmake(worker->position_states.parent(worker->ply));
        --worker->ply;
        return score;
    }

    uint64_t workerKey() const { return worker->board.key(); }

    std::optional<TT_Record> workerTtRecord() const { return tt.probe(workerKey()); }

    void expectNoWorkerTtRecord() const { EXPECT_FALSE(workerTtRecord().has_value()); }

    uint64_t workerNodes() const { return worker->node_count(); }

    bool workerBoardIsDraw() const { return worker->board.is_draw(); }

    Move rootMove() const { return worker->root.best_move; }

    bool rootCompleted() const { return worker->root.completed; }

    bool rootMoveIsLegal() const { return worker->board.is_legal_move(rootMove()); }

    bool rootPvEmpty() const { return worker->root.pv.empty(); }

    int rootPvSize() const { return worker->root.pv.size(); }

    Move rootPvFront() const { return worker->root.pv.front(); }

    int rootValue() const { return worker->root.value; }

    int rootDepth() const { return worker->root.depth; }

    const std::vector<RootLine>& rootLines() const { return worker->root_lines; }

    RootLine rootSnapshot() const { return worker->root_snapshot(); }

    int workerPly() const { return worker->ply; }

    void setWorkerPly(int search_ply) { worker->ply = search_ply; }

#if LATRUNCULI_SEARCH_STATS
    uint64_t statNodesAt(int search_ply) const {
        return worker->stats.raw_counters().nodes[search_ply];
    }

    uint64_t qnodesAt(int search_ply) const {
        return worker->stats.raw_counters().qnodes[search_ply];
    }

    uint64_t mainTtProbesAt(int search_ply) const {
        return worker->stats.raw_counters().main_tt_probes[search_ply];
    }

    uint64_t mainTtHitsAt(int search_ply) const {
        return worker->stats.raw_counters().main_tt_hits[search_ply];
    }

    uint64_t mainTtCutoffsAt(int search_ply) const {
        return worker->stats.raw_counters().main_tt_cutoffs[search_ply];
    }

    uint64_t qTtProbesAt(int search_ply) const {
        return worker->stats.raw_counters().q_tt_probes[search_ply];
    }

    uint64_t qTtHitsAt(int search_ply) const {
        return worker->stats.raw_counters().q_tt_hits[search_ply];
    }

    uint64_t qTtCutoffsAt(int search_ply) const {
        return worker->stats.raw_counters().q_tt_cutoffs[search_ply];
    }

    uint64_t aspirationFailLows() const {
        return worker->stats.raw_counters().aspiration_fail_lows;
    }

    uint64_t aspirationFailHighs() const {
        return worker->stats.raw_counters().aspiration_fail_highs;
    }

    uint64_t aspirationResearches() const {
        return worker->stats.raw_counters().aspiration_researches;
    }
#endif

    void testSearch(const std::string& fen, int search_depth, int score, std::string move) {
        TestBoard board{fen};
        options.board = &board;
        options.depth = search_depth;
        ThreadTestAccess::start_search(*thread, options);
        ThreadTestAccess::wait_for_idle(*thread);

        EXPECT_EQ(worker->root.value, score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(worker->root.best_move.str(), move) << fen;
        }
    }
};

TEST_F(SearchTest, BasicMateAndStalemateTerminals) {
    testSearch("7R/8/8/8/8/1K6/8/1k6 w - - 0 1", 2, MATE_VALUE - 1, "h8h1");
    testSearch("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1", 1, -MATE_VALUE, "none");
    testSearch("k7/8/KQ6/8/8/8/8/8 b - - 0 1", 1, DRAW_VALUE, "none");
}

TEST_F(SearchTest, DrawByRuleReturnsDrawValue) {
    constexpr auto drawn_by_fifty_move = "k7/8/2K5/8/8/8/8/8 b - - 100 1";
    TestBoard      board{drawn_by_fifty_move};
    loadWorkerBoard(board);

    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, 2), DRAW_VALUE);
}

TEST_F(SearchTest, DepthZeroEntersQuiescenceAndSearchesWinningCapture) {
    constexpr auto winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    TestBoard      board{winning_capture};
    loadWorkerBoard(board);

    const int static_eval = evaluate(board);
    EXPECT_GT(runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, 0), static_eval);
    EXPECT_GT(workerNodes(), 1U);
}

TEST_F(SearchTest, DepthZeroDrawByRuleReturnsDrawBeforeStaticEval) {
    constexpr auto drawn_winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 100 1";
    TestBoard      board{drawn_winning_capture};
    loadWorkerBoard(board);

    EXPECT_NE(evaluate(board), DRAW_VALUE);
    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, 0), DRAW_VALUE);
    EXPECT_EQ(workerNodes(), 1U);
}

TEST_F(SearchTest, QuiescenceDrawByRuleDoesNotStoreTranspositionTableEntry) {
    constexpr auto drawn_winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 100 1";
    TestBoard      board{drawn_winning_capture};
    loadWorkerBoard(board);

    EXPECT_EQ(runQuiescence(-INF_VALUE, INF_VALUE), DRAW_VALUE);
    EXPECT_EQ(workerNodes(), 1U);
#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(qnodesAt(0), 1U);
#endif
    expectNoWorkerTtRecord();
}

TEST_F(SearchTest, QuiescenceAtMaxPlyDoesNotStoreTranspositionTableEntry) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);
    setWorkerPly(MAX_SEARCH_PLY);

    const int static_eval = evaluate(board);

    EXPECT_EQ(runQuiescence(-INF_VALUE, INF_VALUE), static_eval);
    EXPECT_EQ(workerNodes(), 1U);
    expectNoWorkerTtRecord();
}

TEST_F(SearchTest, QuiescenceStandPatFailHighReturnsEvalAndStoresLowerbound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    const int static_eval = evaluate(board);
    const int value       = runQuiescence(static_eval - 100, static_eval);

    EXPECT_EQ(value, static_eval);
    EXPECT_EQ(workerNodes(), 1U);

    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->depth, 0);
    EXPECT_EQ(record->flag, TT_Flag::Lowerbound);
    EXPECT_EQ(record->move, NULL_MOVE);
}

TEST_F(SearchTest, QuiescenceInCheckSearchesLegalEvasion) {
    constexpr auto one_legal_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  alpha             = -INF_VALUE;
    constexpr int  beta              = INF_VALUE;

    const int expected = -testQuiescenceAfterMove(one_legal_evasion, "a8b8", -beta, -alpha);

    TestBoard board{one_legal_evasion};
    loadWorkerBoard(board);
    ASSERT_TRUE(board.is_check()) << one_legal_evasion;

    EXPECT_EQ(runQuiescence(alpha, beta), expected);
}

TEST_F(SearchTest, QuiescenceBuildsPrincipalVariationFromTacticalMove) {
    constexpr auto winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    TestBoard      board{winning_capture};
    loadWorkerBoard(board);

    PrincipalVariation pv;
    const int          static_eval = evaluate(board);

    EXPECT_GT(runQuiescenceWithPv(-INF_VALUE, INF_VALUE, pv), static_eval);
    ASSERT_FALSE(pv.empty());
    EXPECT_TRUE(board.is_legal_move(pv.front()));
}

TEST_F(SearchTest, QuiescenceUsesExactTranspositionTableCutoff) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  tt_score      = 321;
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    tt.store_search(workerKey(), NULL_MOVE, tt_score, 0, TT_Flag::Exact, workerPly());

    EXPECT_EQ(runQuiescence(-INF_VALUE, INF_VALUE), tt_score);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(qTtProbesAt(0), 1U);
    EXPECT_EQ(qTtHitsAt(0), 1U);
    EXPECT_EQ(qTtCutoffsAt(0), 1U);
#endif
}

TEST_F(SearchTest, QuiescenceUsesLowerboundTranspositionTableCutoff) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  tt_score      = 500;
    constexpr int  alpha         = 100;
    constexpr int  beta          = 200;
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    tt.store_search(workerKey(), NULL_MOVE, tt_score, 0, TT_Flag::Lowerbound, workerPly());

    EXPECT_EQ(runQuiescence(alpha, beta), tt_score);
}

TEST_F(SearchTest, QuiescenceUsesUpperboundTranspositionTableCutoff) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  tt_score      = -500;
    constexpr int  alpha         = -200;
    constexpr int  beta          = -100;
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    tt.store_search(workerKey(), NULL_MOVE, tt_score, 0, TT_Flag::Upperbound, workerPly());

    EXPECT_EQ(runQuiescence(alpha, beta), tt_score);
}

TEST_F(SearchTest, QuiescenceIgnoresQuietNonCheckTranspositionTableMove) {
    TestBoard baseline_board{POS3};
    loadWorkerBoard(baseline_board);
    const int baseline = runQuiescence(-INF_VALUE, INF_VALUE);

    TestBoard board{POS3};
    loadWorkerBoard(board);

    Move quiet_tt_move = Move(E2, E3);
    ASSERT_TRUE(board.is_pseudo_legal(quiet_tt_move));
    ASSERT_FALSE(board.is_capture(quiet_tt_move));
    tt.store_search(
        workerKey(), quiet_tt_move, -INF_VALUE + 1000, 0, TT_Flag::Lowerbound, workerPly());

    EXPECT_EQ(runQuiescence(-INF_VALUE, INF_VALUE), baseline);
}

TEST_F(SearchTest, QuiescenceCaptureBetaCutoffStoresLowerboundMove) {
    constexpr auto winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";

    TestBoard exact_board{winning_capture};
    loadWorkerBoard(exact_board);
    const int static_eval = evaluate(exact_board);
    const int exact       = runQuiescence(-INF_VALUE, INF_VALUE);
    ASSERT_GT(exact, static_eval);

    TestBoard board{winning_capture};
    loadWorkerBoard(board);

    const int beta  = static_eval + std::max(1, (exact - static_eval) / 2);
    const int alpha = beta - 100;
    const int value = runQuiescence(alpha, beta);
    ASSERT_GE(value, beta);

    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->depth, 0);
    EXPECT_EQ(record->flag, TT_Flag::Lowerbound);
    EXPECT_FALSE(record->move.is_null());
    EXPECT_TRUE(board.is_capture(record->move) || record->move.type() == MOVE_PROM);
}

TEST_F(SearchTest, QuiescenceStoresUpperboundOnFailLow) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    const int static_eval = evaluate(board);
    const int value       = runQuiescence(static_eval + 1, static_eval + 100);
    ASSERT_LT(value, static_eval + 1);

    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->depth, 0);
    EXPECT_EQ(record->flag, TT_Flag::Upperbound);
    EXPECT_EQ(record->move, NULL_MOVE);
}

TEST_F(SearchTest, QuiescenceStoresExactOnNormalExit) {
    constexpr auto winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    TestBoard      board{winning_capture};
    loadWorkerBoard(board);

    const int static_eval = evaluate(board);
    const int value       = runQuiescence(static_eval - 1, INF_VALUE);
    ASSERT_GT(value, static_eval);

    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->depth, 0);
    EXPECT_EQ(record->flag, TT_Flag::Exact);
    EXPECT_FALSE(record->move.is_null());
}

TEST_F(SearchTest, QuiescenceCheckmateWithNoEvasionsReturnsMateAndStoresExact) {
    constexpr auto checkmate  = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1";
    constexpr int  search_ply = 3;
    TestBoard      board{checkmate};
    loadWorkerBoard(board);
    setWorkerPly(search_ply);

    const int value = runQuiescence(-INF_VALUE, INF_VALUE);
    EXPECT_EQ(value, -MATE_VALUE + search_ply);

    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->score, -MATE_VALUE);
    EXPECT_EQ(record->depth, 0);
    EXPECT_EQ(record->flag, TT_Flag::Exact);
    EXPECT_EQ(record->move, NULL_MOVE);
}

TEST_F(SearchTest, QuiescenceRecordsQNodeInStatsBuild) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    (void)runQuiescence(-INF_VALUE, INF_VALUE);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(statNodesAt(0), 1U);
    EXPECT_EQ(qnodesAt(0), 1U);
#endif
}

TEST_F(SearchTest, RootDrawByRuleStillSearchesForLegalBestMove) {
    constexpr auto drawn_root = "k7/8/2K5/8/8/8/8/8 b - - 100 1";
    TestBoard      board{drawn_root};
    loadWorkerBoard(board, 1);

    ASSERT_TRUE(workerBoardIsDraw());

    EXPECT_EQ(runWorkerSearch(), DRAW_VALUE);
    EXPECT_TRUE(rootCompleted());
    EXPECT_FALSE(rootMove().is_null());
    EXPECT_TRUE(rootMoveIsLegal());
    EXPECT_GT(workerNodes(), 1U);

    const RootLine snapshot = rootSnapshot();
    EXPECT_TRUE(snapshot.usable_best_move());
    EXPECT_EQ(snapshot.best_move, rootMove());
}

TEST_F(SearchTest, AlphaBetaFailLowReturnsBestMoveScoreInsteadOfAlpha) {
    constexpr auto one_legal_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  search_depth      = 1;
    constexpr int  alpha             = -100;
    constexpr int  beta              = 0;

    const int expected =
        -testAlphaBetaAfterMove(one_legal_evasion, "a8b8", -beta, -alpha, search_depth - 1);
    ASSERT_LT(expected, alpha) << one_legal_evasion;

    TestBoard board{one_legal_evasion};
    loadWorkerBoard(board);
    ASSERT_TRUE(board.is_check()) << one_legal_evasion;

    const int actual = runNonPvAlphaBeta(alpha, beta, search_depth);
    EXPECT_EQ(actual, expected) << one_legal_evasion;
    EXPECT_NE(actual, alpha) << "fail-low should return the discovered child score, not alpha";
}

TEST_F(SearchTest, AlphaBetaFailHighReturnsRawScoreInsteadOfBeta) {
    constexpr auto one_legal_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  search_depth      = 1;

    TestBoard exact_board{one_legal_evasion};
    loadWorkerBoard(exact_board);
    const int exact = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth);

    const int beta  = exact - 50;
    const int alpha = beta - 100;
    const int expected =
        -testAlphaBetaAfterMove(one_legal_evasion, "a8b8", -beta, -alpha, search_depth - 1);
    ASSERT_GT(expected, beta) << one_legal_evasion;

    TestBoard board{one_legal_evasion};
    loadWorkerBoard(board);
    ASSERT_TRUE(board.is_check()) << one_legal_evasion;

    const int actual = runNonPvAlphaBeta(alpha, beta, search_depth);
    EXPECT_EQ(actual, expected) << one_legal_evasion;
    EXPECT_NE(actual, beta) << "fail-high should return the raw child score, not beta";
}

TEST_F(SearchTest, AlphaBetaMateDistanceLowerClampReturnsBound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    constexpr int search_ply  = 4;
    constexpr int lower_bound = -MATE_VALUE + search_ply;
    setWorkerPly(search_ply);

    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, lower_bound - 1, 1), lower_bound);
}

TEST_F(SearchTest, AlphaBetaMateDistanceUpperClampReturnsBound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    constexpr int search_ply      = 4;
    constexpr int upper_bound     = MATE_VALUE - search_ply - 1;
    constexpr int collapsed_alpha = upper_bound + 1;
    setWorkerPly(search_ply);

    EXPECT_EQ(runNonPvAlphaBeta(collapsed_alpha, INF_VALUE, 1), collapsed_alpha);
}

TEST_F(SearchTest, AlphaBetaAtMaxPlyReturnsStaticEval) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    const int static_eval = evaluate(board);
    setWorkerPly(MAX_SEARCH_PLY);

    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, 1), static_eval);
    EXPECT_EQ(workerNodes(), 1U);
}

TEST_F(SearchTest, AlphaBetaAtMaxPlyReturnsDrawBeforeStaticEval) {
    constexpr auto drawn_winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 100 1";
    TestBoard      board{drawn_winning_capture};
    loadWorkerBoard(board);

    EXPECT_NE(evaluate(board), DRAW_VALUE);
    setWorkerPly(MAX_SEARCH_PLY);

    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, 1), DRAW_VALUE);
    EXPECT_EQ(workerNodes(), 1U);
}

TEST_F(SearchTest, AlphaBetaUsesExactTranspositionTableCutoff) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;
    constexpr int  tt_score      = 321;

    TestBoard board{quiet_control};
    loadWorkerBoard(board);

    tt.store_search(workerKey(), NULL_MOVE, tt_score, search_depth, TT_Flag::Exact, workerPly());

    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth), tt_score);
}

TEST_F(SearchTest, AlphaBetaUsesLowerboundTranspositionTableCutoff) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;
    constexpr int  tt_score      = 500;
    constexpr int  alpha         = 100;
    constexpr int  beta          = 200;

    TestBoard board{quiet_control};
    loadWorkerBoard(board);

    tt.store_search(
        workerKey(), NULL_MOVE, tt_score, search_depth, TT_Flag::Lowerbound, workerPly());

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth), tt_score);
}

TEST_F(SearchTest, AlphaBetaUsesUpperboundTranspositionTableCutoff) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;
    constexpr int  tt_score      = -500;
    constexpr int  alpha         = -200;
    constexpr int  beta          = -100;

    TestBoard board{quiet_control};
    loadWorkerBoard(board);

    tt.store_search(
        workerKey(), NULL_MOVE, tt_score, search_depth, TT_Flag::Upperbound, workerPly());

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth), tt_score);
}

TEST_F(SearchTest, AlphaBetaDoesNotCutoffWithShallowTranspositionTableEntry) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;

    TestBoard baseline_board{quiet_control};
    loadWorkerBoard(baseline_board);
    const int baseline = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth);

    TestBoard board{quiet_control};
    loadWorkerBoard(board);

    Move invalid_tt_move{H1, H2};
    ASSERT_FALSE(board.is_pseudo_legal(invalid_tt_move));

    tt.store_search(
        workerKey(), invalid_tt_move, baseline + 500, search_depth - 1, TT_Flag::Exact, 0);

    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth), baseline);
}

TEST_F(SearchTest, AlphaBetaStoresExactTranspositionTableEntry) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    const uint64_t key = workerKey();
    ASSERT_FALSE(tt.probe(key).has_value());

    const int value = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth);

    auto record = tt.probe(key);
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->depth, search_depth);
    EXPECT_EQ(record->flag, TT_Flag::Exact);
    EXPECT_FALSE(record->move.is_null());
    EXPECT_TRUE(board.is_legal_move(record->move));
}

TEST_F(SearchTest, AlphaBetaStoresLowerboundOnBetaCutoff) {
    constexpr auto one_legal_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  search_depth      = 1;

    TestBoard exact_board{one_legal_evasion};
    loadWorkerBoard(exact_board);
    const int exact = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth);

    TestBoard board{one_legal_evasion};
    loadWorkerBoard(board);

    const int beta  = exact - 50;
    const int alpha = beta - 100;
    const int value = runNonPvAlphaBeta(alpha, beta, search_depth);
    ASSERT_GE(value, beta);

    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->depth, search_depth);
    EXPECT_EQ(record->flag, TT_Flag::Lowerbound);
    EXPECT_FALSE(record->move.is_null());
}

TEST_F(SearchTest, AlphaBetaStoresUpperboundOnFailLow) {
    constexpr auto one_legal_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  search_depth      = 1;
    constexpr int  alpha             = -100;
    constexpr int  beta              = 0;

    TestBoard board{one_legal_evasion};
    loadWorkerBoard(board);

    const int value = runNonPvAlphaBeta(alpha, beta, search_depth);
    ASSERT_LT(value, alpha);

    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->depth, search_depth);
    EXPECT_EQ(record->flag, TT_Flag::Upperbound);
    EXPECT_FALSE(record->move.is_null());
}

TEST_F(SearchTest, AlphaBetaStoresNoLegalMoveTerminalAsExactTranspositionTableEntry) {
    constexpr auto checkmate    = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth = 2;
    constexpr int  search_ply   = 3;
    TestBoard      board{checkmate};
    loadWorkerBoard(board);
    setWorkerPly(search_ply);

    const int value = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth);
    ASSERT_EQ(value, -MATE_VALUE + search_ply);

    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->score_at_ply(workerPly()), value);
    EXPECT_EQ(record->score, -MATE_VALUE);
    EXPECT_EQ(record->depth, search_depth);
    EXPECT_EQ(record->flag, TT_Flag::Exact);
    EXPECT_EQ(record->move, NULL_MOVE);
}

TEST_F(SearchTest, RootSearchDoesNotStoreRootPositionTranspositionTableEntry) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 1);

    const uint64_t root_key = workerKey();
    (void)runRootSearch();

    EXPECT_FALSE(tt.probe(root_key).has_value());
}

TEST_F(SearchTest, AlphaBetaMainTTCutoffDoesNotEnterQSearchTT) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;
    constexpr int  tt_score      = 321;
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    tt.store_search(workerKey(), NULL_MOVE, tt_score, search_depth, TT_Flag::Exact, workerPly());
    (void)runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(mainTtProbesAt(0), 1U);
    EXPECT_EQ(mainTtHitsAt(0), 1U);
    EXPECT_EQ(mainTtCutoffsAt(0), 1U);
    EXPECT_EQ(qTtProbesAt(0), 0U);
    EXPECT_EQ(qTtHitsAt(0), 0U);
    EXPECT_EQ(qTtCutoffsAt(0), 0U);
#endif
}

TEST_F(SearchTest, ResetDoesNotSeedRootLineWithFallbackMove) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board);

    EXPECT_EQ(rootMove(), NULL_MOVE);
    EXPECT_EQ(rootValue(), evaluate(board));
    EXPECT_EQ(rootDepth(), 0);
    EXPECT_TRUE(rootPvEmpty());
}

TEST_F(SearchTest, RootLineListContainsLegalGeneratedRootMoves) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 1);

    int legal_move_count = 0;
    for (Move move : movegen::generate_pseudo_legal(board)) {
        if (board.is_legal_generated_move(move))
            ++legal_move_count;
    }

    buildRootLines();

    ASSERT_EQ(rootLines().size(), static_cast<size_t>(legal_move_count));
    for (const RootLine& line : rootLines()) {
        EXPECT_FALSE(line.best_move.is_null());
        EXPECT_TRUE(board.is_legal_generated_move(line.best_move));
        EXPECT_FALSE(line.completed);
        EXPECT_EQ(line.depth, 0);
        EXPECT_TRUE(line.pv.empty());
    }
}

TEST_F(SearchTest, RootSearchCompletesAndOrdersRootLinesByBestCompletedLine) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 2);

    (void)runRootSearch();

    ASSERT_TRUE(rootCompleted());
    ASSERT_TRUE(rootMoveIsLegal());
    ASSERT_FALSE(rootLines().empty());
    EXPECT_EQ(rootLines().front().best_move, rootMove());
    EXPECT_EQ(rootLines().front().value, rootValue());
    EXPECT_EQ(rootLines().front().depth, rootDepth());
    EXPECT_EQ(rootDepth(), 2);

    for (size_t index = 1; index < rootLines().size(); ++index)
        EXPECT_FALSE(is_better_root_line(rootLines()[index], rootLines()[index - 1]));
}

TEST_F(SearchTest, RootSearchBuildsPrincipalVariationForRootLine) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 2);

    (void)runRootSearch();

    ASSERT_TRUE(rootCompleted());
    ASSERT_FALSE(rootPvEmpty());
    EXPECT_EQ(rootPvFront(), rootMove());
    EXPECT_EQ(rootPvSize(), 2);
}

TEST_F(SearchTest, RootAspirationWidensAfterFailHighAndCompletes) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 1);

    ASSERT_TRUE(runRootAspiration(1, -1000));

    EXPECT_TRUE(rootCompleted());
    EXPECT_TRUE(rootMoveIsLegal());
    EXPECT_EQ(rootDepth(), 1);
    EXPECT_GT(rootValue(), -950);
    ASSERT_FALSE(rootPvEmpty());
    EXPECT_EQ(rootPvFront(), rootMove());

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(aspirationFailHighs(), 0U);
    EXPECT_EQ(aspirationFailLows(), 0U);
    EXPECT_EQ(aspirationResearches(), aspirationFailHighs());
#endif
}

TEST_F(SearchTest, RootAspirationWidensAfterFailLowAndCompletes) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 1);

    ASSERT_TRUE(runRootAspiration(1, 1000));

    EXPECT_TRUE(rootCompleted());
    EXPECT_TRUE(rootMoveIsLegal());
    EXPECT_EQ(rootDepth(), 1);
    EXPECT_LT(rootValue(), 950);
    ASSERT_FALSE(rootPvEmpty());
    EXPECT_EQ(rootPvFront(), rootMove());

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(aspirationFailLows(), 0U);
    EXPECT_EQ(aspirationFailHighs(), 0U);
    EXPECT_EQ(aspirationResearches(), aspirationFailLows());
#endif
}

TEST_F(SearchTest, StoppedRootAspirationPreservesLastAcceptedRootSnapshot) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 2);

    ASSERT_TRUE(runRootAspiration(1, evaluate(board)));
    const RootLine accepted = rootSnapshot();
    ASSERT_TRUE(accepted.usable_best_move());
    ASSERT_EQ(accepted.depth, 1);

    ThreadTestAccess::request_stop(*thread);

    EXPECT_FALSE(runRootAspiration(2, accepted.value));

    const RootLine snapshot = rootSnapshot();
    EXPECT_EQ(snapshot.best_move, accepted.best_move);
    EXPECT_EQ(snapshot.value, accepted.value);
    EXPECT_EQ(snapshot.depth, accepted.depth);
    EXPECT_EQ(snapshot.completed, accepted.completed);
    EXPECT_EQ(snapshot.pv.size(), accepted.pv.size());
    ASSERT_FALSE(snapshot.pv.empty());
    EXPECT_EQ(snapshot.pv.front(), accepted.best_move);
}

TEST_F(SearchTest, RootSearchSetsNullMoveForCheckmate) {
    constexpr auto checkmate = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1";
    TestBoard      board{checkmate};
    loadWorkerBoard(board);

    EXPECT_EQ(runRootSearch(), -MATE_VALUE);
    EXPECT_EQ(rootMove(), NULL_MOVE);
    EXPECT_EQ(rootValue(), -MATE_VALUE);
    EXPECT_EQ(rootDepth(), options.depth);
    EXPECT_TRUE(rootCompleted());
    EXPECT_TRUE(rootLines().empty());
}

TEST_F(SearchTest, RootSearchSetsNullMoveForStalemate) {
    constexpr auto stalemate = "k7/8/KQ6/8/8/8/8/8 b - - 0 1";
    TestBoard      board{stalemate};
    loadWorkerBoard(board);

    EXPECT_EQ(runRootSearch(), DRAW_VALUE);
    EXPECT_EQ(rootMove(), NULL_MOVE);
    EXPECT_EQ(rootValue(), DRAW_VALUE);
    EXPECT_EQ(rootDepth(), options.depth);
    EXPECT_TRUE(rootCompleted());
    EXPECT_TRUE(rootLines().empty());
}

TEST_F(SearchTest, CompletedSearchPublishesRootLineSnapshot) {
    constexpr auto mate_in_one = "7R/8/8/8/8/1K6/8/1k6 w - - 0 1";
    TestBoard      board{mate_in_one};
    options.board = &board;
    options.depth = 2;

    ThreadTestAccess::start_search(*thread, options);
    ThreadTestAccess::wait_for_idle(*thread);

    const auto snapshot = rootSnapshot();
    EXPECT_TRUE(snapshot.completed);
    EXPECT_TRUE(snapshot.completed_depth());
    EXPECT_TRUE(snapshot.usable_best_move());
    EXPECT_EQ(snapshot.best_move.str(), "h8h1");
    ASSERT_FALSE(snapshot.pv.empty());
    EXPECT_EQ(snapshot.pv.front(), snapshot.best_move);
    EXPECT_EQ(snapshot.pv.size(), 1);
    EXPECT_EQ(snapshot.value, MATE_VALUE - 1);
    EXPECT_EQ(snapshot.depth, 2);
    EXPECT_GT(workerNodes(), 0U);
}

TEST_F(SearchTest, CompletedNonTerminalSearchPublishesFullRootPv) {
    TestBoard board{STARTFEN};
    options.board = &board;
    options.depth = 2;

    ThreadTestAccess::start_search(*thread, options);
    ThreadTestAccess::wait_for_idle(*thread);

    const auto snapshot = rootSnapshot();
    ASSERT_TRUE(snapshot.completed);
    ASSERT_TRUE(snapshot.usable_best_move());
    ASSERT_EQ(snapshot.pv.size(), 2);
    EXPECT_EQ(snapshot.pv.front(), snapshot.best_move);
}

TEST_F(SearchTest, CompletedNoLegalMoveSearchPublishesCompletedNullMove) {
    constexpr auto checkmate = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1";
    TestBoard      board{checkmate};
    options.board = &board;
    options.depth = 1;

    ThreadTestAccess::start_search(*thread, options);
    ThreadTestAccess::wait_for_idle(*thread);

    const auto snapshot = rootSnapshot();
    EXPECT_TRUE(snapshot.completed);
    EXPECT_TRUE(snapshot.completed_depth());
    EXPECT_FALSE(snapshot.usable_best_move());
    EXPECT_EQ(snapshot.best_move, NULL_MOVE);
    EXPECT_EQ(snapshot.value, -MATE_VALUE);
    EXPECT_EQ(snapshot.depth, 1);
    EXPECT_TRUE(snapshot.pv.empty());
}

TEST_F(SearchTest, StoppedSearchPublishesIncompleteRootLineSnapshot) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadWorkerBoard(board, 2);

    ThreadTestAccess::request_stop(*thread);
    (void)runWorkerSearch();

    const auto snapshot = rootSnapshot();
    EXPECT_FALSE(snapshot.completed);
    EXPECT_EQ(snapshot.best_move, NULL_MOVE);
    EXPECT_EQ(snapshot.depth, 0);
    EXPECT_FALSE(snapshot.completed_depth());
    EXPECT_FALSE(snapshot.usable_best_move());
    EXPECT_TRUE(snapshot.pv.empty());
}

TEST_F(SearchTest, StoppedSearchPreservesLastCompletedRootLine) {
    TestBoard board{STARTFEN};
    options.board = &board;
    options.depth = 8;
    options.nodes = 100;

    ThreadTestAccess::start_search(*thread, options);
    ThreadTestAccess::wait_for_idle(*thread);

    const auto snapshot = rootSnapshot();
    ASSERT_TRUE(snapshot.completed_depth());
    EXPECT_TRUE(snapshot.usable_best_move());
    EXPECT_LT(snapshot.depth, options.depth);
    ASSERT_FALSE(snapshot.pv.empty());
    EXPECT_EQ(snapshot.pv.front(), snapshot.best_move);
}

TEST_F(SearchTest, StoppedSearchReturnsAbortSentinelValue) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  alpha         = -123;
    constexpr int  beta          = 456;

    TestBoard board{quiet_control};
    loadWorkerBoard(board);
    ThreadTestAccess::request_stop(*thread);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, 2), alpha);
}

TEST_F(SearchTest, SelectBestRootLinePrefersHigherCompletedDepth) {
    const RootLine fallback{.best_move = Move(E2, E4), .value = 50, .depth = 2, .completed = true};
    const std::array<RootLine, 2> lines{
        RootLine{.best_move = Move(G1, F3), .value = 0, .depth = 3, .completed = true},
        RootLine{.best_move = Move(D2, D4), .value = 200, .depth = 1, .completed = true},
    };

    const RootLine selected = select_best_root_line(fallback, lines);

    EXPECT_EQ(selected.best_move, Move(G1, F3));
    EXPECT_EQ(selected.depth, 3);
}

TEST_F(SearchTest, SelectBestRootLineTieBreaksByHigherValue) {
    const RootLine fallback{.best_move = Move(E2, E4), .value = 10, .depth = 3, .completed = true};
    const std::array<RootLine, 2> lines{
        RootLine{.best_move = Move(G1, F3), .value = 25, .depth = 3, .completed = true},
        RootLine{.best_move = Move(D2, D4), .value = 20, .depth = 3, .completed = true},
    };

    const RootLine selected = select_best_root_line(fallback, lines);

    EXPECT_EQ(selected.best_move, Move(G1, F3));
    EXPECT_EQ(selected.value, 25);
}

TEST_F(SearchTest, SelectBestRootLineTieBreaksByLowerMoveBits) {
    const RootLine fallback{.best_move = Move(H2, H3), .value = 10, .depth = 3, .completed = true};
    const RootLine first{.best_move = Move(G2, G3), .value = 10, .depth = 3, .completed = true};
    const RootLine second{.best_move = Move(A2, A3), .value = 10, .depth = 3, .completed = true};
    const std::array<RootLine, 2> lines{first, second};

    const RootLine selected = select_best_root_line(fallback, lines);
    const Move     expected =
        first.best_move.bits < second.best_move.bits ? first.best_move : second.best_move;

    EXPECT_LT(expected.bits, fallback.best_move.bits);
    EXPECT_EQ(selected.best_move, expected);
}

TEST_F(SearchTest, SelectBestRootLineIgnoresUnusableLines) {
    const RootLine fallback{.best_move = Move(E2, E4), .value = 10, .depth = 2, .completed = true};
    const std::array<RootLine, 3> lines{
        RootLine{.best_move = Move(G1, F3), .value = 1000, .depth = 99, .completed = false},
        RootLine{.best_move = NULL_MOVE, .value = 1000, .depth = 99, .completed = true},
        RootLine{.best_move = Move(D2, D4), .value = 1000, .depth = 0, .completed = true},
    };

    const RootLine selected = select_best_root_line(fallback, lines);

    EXPECT_EQ(selected.best_move, fallback.best_move);
    EXPECT_EQ(selected.value, fallback.value);
    EXPECT_EQ(selected.depth, fallback.depth);
}

TEST_F(SearchTest, SelectBestRootLineReturnsFallbackWhenNoUsableLineExists) {
    const RootLine fallback{
        .best_move = NULL_MOVE, .value = DRAW_VALUE, .depth = 1, .completed = true};
    const std::array<RootLine, 2> lines{
        RootLine{.best_move = Move(G1, F3), .value = 1000, .depth = 2, .completed = false},
        RootLine{.best_move = NULL_MOVE, .value = -MATE_VALUE, .depth = 2, .completed = true},
    };

    const RootLine selected = select_best_root_line(fallback, lines);

    EXPECT_TRUE(selected.best_move.is_null());
    EXPECT_TRUE(selected.completed);
    EXPECT_TRUE(selected.completed_depth());
    EXPECT_FALSE(selected.usable_best_move());
    EXPECT_EQ(selected.value, DRAW_VALUE);
}
