#include <algorithm>
#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "board.hpp"
#include "evaluator.hpp"
#include "move_picker.hpp"
#include "movegen.hpp"
#include "search_limits.hpp"
#include "search_worker.hpp"
#include "test_util.hpp"
#include "thread_test_access.hpp"
#include "threading.hpp"
#include "tt.hpp"
#include "uci.hpp"

namespace {

constexpr auto AnyMove = "ANY";

PrincipalVariation pv_for_move(Move move) {
    PrincipalVariation pv;
    PrincipalVariation child;
    pv.update(move, child);
    return pv;
}

PrincipalVariation pv_for_line(Move first, Move second) {
    PrincipalVariation child = pv_for_move(second);
    PrincipalVariation pv;
    pv.update(first, child);
    return pv;
}

} // namespace

class SearchTest : public ::testing::Test {
protected:
    std::ostringstream protocol_out;
    std::ostringstream protocol_err;
    uci::Writer        writer{protocol_out, protocol_err};
    ThreadPool         pool{1, writer};
    Thread*            thread;
    SearchWorker*      worker;
    SearchLimits       limits;

    void SetUp() override {
        thread       = &ThreadTestAccess::thread(pool);
        worker       = &ThreadTestAccess::worker(pool);
        limits       = SearchLimits();
        limits.depth = 4;
        tt.clear();
    }

    void loadWorkerBoard(Board& board, int search_depth = 4) {
        tt.clear();
        limits.depth = search_depth;
        worker->configure_search(board, limits, Clock::now());
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

    int runNonPvAlphaBeta(int alpha, int beta, int search_depth, bool can_null = true) {
        return worker->alphabeta<NON_PV>(alpha, beta, search_depth, nullptr, can_null);
    }

    int runPvAlphaBeta(int alpha, int beta, int search_depth, PrincipalVariation& pv) {
        return worker->alphabeta<PV>(alpha, beta, search_depth, &pv);
    }

    void buildRootLines() { worker->build_root_lines(); }

    int runRootSearch() {
        worker->build_root_lines();
        return worker->search_root();
    }

    bool runRootDepth(int search_depth, int previous_value) {
        worker->build_root_lines();
        return worker->search_root_depth(search_depth, previous_value);
    }

    bool runRootWindow(int search_depth, int alpha, int beta) {
        return worker->search_root_window(search_depth, alpha, beta);
    }

    int runQuiescence(int alpha, int beta) { return worker->quiescence<NON_PV>(alpha, beta); }

    int runPvQuiescence(int alpha, int beta, PrincipalVariation& pv) {
        return worker->quiescence<PV>(alpha, beta, &pv);
    }

    int runWorkerSearch() { return worker->search(); }

    void reportRootProgress(const RootLine& line) {
        worker->report_root_progress(line);
    }

    int count_protocol_lines_starting_with(std::string_view prefix) const {
        std::istringstream lines{protocol_out.str()};
        std::string        line;
        int                count = 0;

        while (std::getline(lines, line)) {
            if (line.starts_with(prefix))
                ++count;
        }

        return count;
    }

    template <typename Fn>
    auto withWorkerMove(Move move, Fn&& fn) {
        using Result = std::invoke_result_t<Fn&>;

        if (move.is_null()) {
            ADD_FAILURE() << "withWorkerMove requires a non-null move";
            if constexpr (std::is_void_v<Result>) {
                return;
            } else {
                return Result{};
            }
        }

        worker->board.make(move, worker->position_states.child(worker->ply));
        ++worker->ply;

        if constexpr (std::is_void_v<Result>) {
            fn();
            worker->board.unmake(worker->position_states.parent(worker->ply));
            --worker->ply;
        } else {
            Result result = fn();
            worker->board.unmake(worker->position_states.parent(worker->ply));
            --worker->ply;
            return result;
        }
    }

    template <typename Fn>
    auto withWorkerNullMove(Fn&& fn) {
        using Result = std::invoke_result_t<Fn&>;

        worker->board.make_null(worker->position_states.child(worker->ply));
        ++worker->ply;

        if constexpr (std::is_void_v<Result>) {
            fn();
            worker->board.unmake_null(worker->position_states.parent(worker->ply));
            --worker->ply;
        } else {
            Result result = fn();
            worker->board.unmake_null(worker->position_states.parent(worker->ply));
            --worker->ply;
            return result;
        }
    }

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

        return withWorkerMove(move,
                              [&] { return worker->alphabeta<NON_PV>(alpha, beta, search_depth); });
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

        return withWorkerMove(move, [&] { return worker->quiescence<NON_PV>(alpha, beta); });
    }

    void storeQsearchTtAfterMove(Move move, int score, TT_Flag flag) {
        ASSERT_FALSE(move.is_null());

        withWorkerMove(
            move, [&] { tt.store_search(workerKey(), NULL_MOVE, score, 0, flag, workerPly()); });
    }

    Move firstWorkerPickerMove(Move tt_move = NULL_MOVE) {
        const auto context = MoveOrdering::make_context(worker->board);
        MovePicker picker =
            MovePicker::main_search(worker->board, worker->ordering, context, worker->ply, tt_move);
        for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
            if (worker->board.is_legal_generated_move(move))
                return move;
        }
        return NULL_MOVE;
    }

    std::vector<Move> workerPickerLegalMoves(Move tt_move = NULL_MOVE) {
        std::vector<Move> moves;
        const auto        context = MoveOrdering::make_context(worker->board);
        MovePicker        picker =
            MovePicker::main_search(worker->board, worker->ordering, context, worker->ply, tt_move);
        for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
            if (worker->board.is_legal_generated_move(move))
                moves.push_back(move);
        }
        return moves;
    }

    int scoreWorkerChild(Move move, int alpha, int beta, int search_depth) {
        return withWorkerMove(move, [&] {
            return -worker->alphabeta<NON_PV>(-beta, -alpha, search_depth, nullptr, true);
        });
    }

    void storeWorkerChildSearchTt(Move move, int score, int depth, TT_Flag flag) {
        ASSERT_FALSE(move.is_null());

        withWorkerMove(move, [&] {
            tt.store_search(workerKey(), NULL_MOVE, score, depth, flag, workerPly());
        });
    }

    bool workerLegalGeneratedMove(Move move) const {
        return worker->board.is_legal_generated_move(move);
    }

    bool workerLegalMove(Move move) const { return worker->board.is_legal_move(move); }

    bool workerIsCapture(Move move) const { return worker->board.is_capture(move); }

    PieceType workerPieceTypeOn(Square sq) const { return worker->board.piecetype_on(sq); }

    Color workerSideToMove() const { return worker->board.side_to_move(); }

    void seedWorkerKiller(Move move) { worker->ordering.killers.update(move, worker->ply); }

    void boostWorkerHistory(Move move, int depth) {
        worker->ordering.quiets.reward(worker->board.side_to_move(), move.from(), move.to(), depth);
    }

    int workerQuietHistory(Move move) const {
        return worker->ordering.quiets.get(worker->board.side_to_move(), move.from(), move.to());
    }

    int
    workerContinuationHistory(Color prev_c, PieceType prev_piece, Square prev_to, Move move) const {
        const PieceType piece = worker->board.piecetype_on(move.from());
        return worker->ordering.continuations.get(prev_c, prev_piece, prev_to, piece, move.to());
    }

    Move workerCounterMove(Color prev_c, PieceType prev_piece, Square prev_to) const {
        return worker->ordering.counters.get(prev_c, prev_piece, prev_to);
    }

    uint64_t workerKey() const { return worker->board.key(); }

    uint64_t workerNullChildKey() const {
        TestBoard board_copy{worker->board.toFEN()};
        board_copy.make_null();
        return board_copy.key();
    }

    uint64_t workerDescendantNullKey(Move move) const {
        TestBoard board_copy{worker->board.toFEN()};
        board_copy.make(move);
        board_copy.make_null();
        return board_copy.key();
    }

    std::optional<TT_Record> workerTtRecord() const { return tt.probe(workerKey()); }

    void expectNoWorkerTtRecord() const { EXPECT_FALSE(workerTtRecord().has_value()); }

    uint64_t workerNodes() const { return worker->node_count(); }

    bool workerBoardIsDraw() const { return worker->board.is_draw(); }

    Move rootMove() const { return worker->root_result.root_move; }

    bool rootCompleted() const { return worker->root_result.completed; }

    bool rootMoveIsLegal() const { return worker->board.is_legal_move(rootMove()); }

    bool rootPvEmpty() const { return worker->root_result.pv.empty(); }

    int rootPvSize() const { return worker->root_result.pv.size(); }

    Move rootPvFront() const { return worker->root_result.pv.front(); }

    int rootValue() const { return worker->root_result.value; }

    int rootDepth() const { return worker->root_result.depth; }

    const std::vector<RootLine>& rootLines() const { return worker->root_lines; }

    std::vector<RootLine>& mutableRootLines() { return worker->root_lines; }

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

    uint64_t pvsResearchesAt(int search_ply) const {
        return worker->stats.raw_counters().pvs_researches[search_ply];
    }

    uint64_t nullMoveTriesAt(int search_ply) const {
        return worker->stats.raw_counters().null_move_tries[search_ply];
    }

    uint64_t nullMoveCutoffsAt(int search_ply) const {
        return worker->stats.raw_counters().null_move_cutoffs[search_ply];
    }

    uint64_t razorTriesAt(int search_ply) const {
        return worker->stats.raw_counters().razor_tries[search_ply];
    }

    uint64_t razorCutoffsAt(int search_ply) const {
        return worker->stats.raw_counters().razor_cutoffs[search_ply];
    }

    uint64_t futilitySkipsAt(int search_ply) const {
        return worker->stats.raw_counters().futility_skips[search_ply];
    }

    uint64_t lmrTriesAt(int search_ply) const {
        return worker->stats.raw_counters().lmr_tries[search_ply];
    }

    uint64_t lmrResearchesAt(int search_ply) const {
        return worker->stats.raw_counters().lmr_researches[search_ply];
    }

    uint64_t quietCutoffsAt(int remaining_depth) const {
        return worker->stats.raw_counters().quiet_cutoffs[remaining_depth];
    }

    uint64_t quietMalusEligibleNodesAt(int remaining_depth) const {
        return worker->stats.raw_counters().quiet_malus_eligible_nodes[remaining_depth];
    }

    uint64_t quietMalusFailedQuietsAt(int remaining_depth) const {
        return worker->stats.raw_counters().quiet_malus_failed_quiets[remaining_depth];
    }

    uint64_t quietMalusUpdatesAt(int remaining_depth) const {
        return worker->stats.raw_counters().quiet_malus_updates[remaining_depth];
    }
#endif

    void testSearch(const std::string& fen, int search_depth, int score, std::string move) {
        TestBoard board{fen};
        limits.depth = search_depth;
        ThreadTestAccess::start_search(*thread, board, limits);
        ThreadTestAccess::wait_for_idle(*thread);

        EXPECT_EQ(rootValue(), score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(rootMove().str(), move) << fen;
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

    EXPECT_GT(runPvQuiescence(-INF_VALUE, INF_VALUE, pv), static_eval);
    ASSERT_FALSE(pv.empty());
    EXPECT_TRUE(board.is_legal_move(pv.front()));
}

TEST_F(SearchTest, QuiescenceUsesTranspositionTableCutoffForEligibleBounds) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";

    struct Case {
        const char* name;
        TT_Flag     flag;
        int         score;
        int         alpha;
        int         beta;
    };

    const std::array cases{
        Case{"exact", TT_Flag::Exact, 321, -INF_VALUE, INF_VALUE},
        Case{"lowerbound", TT_Flag::Lowerbound, 500, 100, 200},
        Case{"upperbound", TT_Flag::Upperbound, -500, -200, -100},
    };

    for (const auto& tc : cases) {
        TestBoard board{quiet_control};
        loadWorkerBoard(board);

        tt.store_search(workerKey(), NULL_MOVE, tc.score, 0, tc.flag, workerPly());

        EXPECT_EQ(runQuiescence(tc.alpha, tc.beta), tc.score) << tc.name;

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(qTtProbesAt(0), 1U) << tc.name;
        EXPECT_EQ(qTtHitsAt(0), 1U) << tc.name;
        EXPECT_EQ(qTtCutoffsAt(0), 1U) << tc.name;
#endif
    }
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
    EXPECT_TRUE(snapshot.usable_root_move());
    EXPECT_EQ(snapshot.root_move, rootMove());
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

TEST_F(SearchTest, AlphaBetaUsesTranspositionTableCutoffForEligibleBounds) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;

    struct Case {
        const char* name;
        TT_Flag     flag;
        int         score;
        int         alpha;
        int         beta;
    };

    const std::array cases{
        Case{"exact", TT_Flag::Exact, 321, -INF_VALUE, INF_VALUE},
        Case{"lowerbound", TT_Flag::Lowerbound, 500, 100, 200},
        Case{"upperbound", TT_Flag::Upperbound, -500, -200, -100},
    };

    for (const auto& tc : cases) {
        TestBoard board{quiet_control};
        loadWorkerBoard(board);

        tt.store_search(workerKey(), NULL_MOVE, tc.score, search_depth, tc.flag, workerPly());

        EXPECT_EQ(runNonPvAlphaBeta(tc.alpha, tc.beta, search_depth), tc.score) << tc.name;
    }
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

TEST_F(SearchTest, AlphaBetaNullMovePruningReturnsFailSoftCutoffFromNullChild) {
    constexpr int search_depth     = 4;
    constexpr int alpha            = -50;
    constexpr int beta             = 50;
    constexpr int null_child_score = -200;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const uint64_t root_key       = workerKey();
    const uint64_t null_child_key = workerNullChildKey();
    ASSERT_FALSE(tt.probe(root_key).has_value());
    ASSERT_FALSE(tt.probe(null_child_key).has_value());

    tt.store_search(
        null_child_key, NULL_MOVE, null_child_score, search_depth - 3, TT_Flag::Exact, 1);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth), -null_child_score);
    EXPECT_FALSE(tt.probe(root_key).has_value());

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(nullMoveTriesAt(0), 1U);
    EXPECT_EQ(nullMoveCutoffsAt(0), 1U);
#endif
}

TEST_F(SearchTest, PvAlphaBetaDoesNotUseNullMovePruning) {
    constexpr int search_depth     = 4;
    constexpr int alpha            = -50;
    constexpr int beta             = 50;
    constexpr int null_child_score = -200;

    TestBoard baseline_board{STARTFEN};
    loadWorkerBoard(baseline_board, search_depth);
    PrincipalVariation baseline_pv;
    const int          baseline = runPvAlphaBeta(alpha, beta, search_depth, baseline_pv);

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);
    const uint64_t null_child_key = workerNullChildKey();
    tt.store_search(
        null_child_key, NULL_MOVE, null_child_score, search_depth - 3, TT_Flag::Exact, 1);

    PrincipalVariation pv;
    EXPECT_EQ(runPvAlphaBeta(alpha, beta, search_depth, pv), baseline);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(nullMoveTriesAt(0), 0U);
    EXPECT_EQ(nullMoveCutoffsAt(0), 0U);
#endif
}

TEST_F(SearchTest, AlphaBetaNullMovePruningRequiresAllGuards) {
    constexpr int alpha            = -50;
    constexpr int beta             = 50;
    constexpr int null_child_score = -200;

    struct Case {
        const char* name;
        const char* fen;
        int         depth;
        bool        can_null;
    };

    const std::array cases{
        Case{"in check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", 4, true},
        Case{"insufficient material", "k7/8/2K5/8/8/8/8/8 b - - 0 1", 4, true},
        Case{"lone rook material", "4k3/8/8/8/8/8/8/4K2R w - - 0 1", 4, true},
        Case{"shallower than reduction", STARTFEN, 2, true},
        Case{"immediate repeated null",
             "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 1",
             4,
             false},
    };

    const auto expectNullMoveBlocked = [&](const Case& tc) {
        SCOPED_TRACE(tc.name);

        TestBoard baseline_board{tc.fen};
        loadWorkerBoard(baseline_board, tc.depth);
        const int baseline = runNonPvAlphaBeta(alpha, beta, tc.depth, tc.can_null);

        TestBoard board{tc.fen};
        loadWorkerBoard(board, tc.depth);
        tt.store_search(workerNullChildKey(),
                        NULL_MOVE,
                        null_child_score,
                        std::max(0, tc.depth - 3),
                        TT_Flag::Exact,
                        1);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, tc.depth, tc.can_null), baseline);

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(nullMoveTriesAt(0), 0U);
        EXPECT_EQ(nullMoveCutoffsAt(0), 0U);
#endif
    };

    for (const auto& tc : cases) {
        expectNullMoveBlocked(tc);
    }
}

TEST_F(SearchTest, AlphaBetaNullMovePruningHonorsParentUpperBoundVeto) {
    constexpr int search_depth     = 4;
    constexpr int alpha            = -50;
    constexpr int beta             = 50;
    constexpr int null_child_score = -200;

    TestBoard baseline_board{STARTFEN};
    loadWorkerBoard(baseline_board, search_depth);
    tt.store_search(
        workerKey(), NULL_MOVE, beta - 1, search_depth, TT_Flag::Upperbound, workerPly());
    tt.store_search(
        workerNullChildKey(), NULL_MOVE, null_child_score, search_depth - 3, TT_Flag::Exact, 1);
    const int baseline = runNonPvAlphaBeta(alpha, beta, search_depth, false);

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);
    tt.store_search(
        workerKey(), NULL_MOVE, beta - 1, search_depth, TT_Flag::Upperbound, workerPly());
    tt.store_search(
        workerNullChildKey(), NULL_MOVE, null_child_score, search_depth - 3, TT_Flag::Exact, 1);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth), baseline);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(nullMoveTriesAt(0), 0U);
    EXPECT_EQ(nullMoveCutoffsAt(0), 0U);
#endif
}

TEST_F(SearchTest, NullMoveDisablesOnlyImmediateChildAndReenablesLaterDescendants) {
    constexpr auto immediate_null_child =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 1";
    constexpr int search_depth = 5;

    TestBoard board{immediate_null_child};
    loadWorkerBoard(board, search_depth);

    ASSERT_FALSE(board.is_check());
    ASSERT_GT(board.nonPawnMaterial(board.side_to_move()), ROOK_MG);

    const Move real_move = findWorkerMove("e7e5");
    ASSERT_FALSE(real_move.is_null());

    const uint64_t immediate_null_key  = workerNullChildKey();
    const uint64_t descendant_null_key = workerDescendantNullKey(real_move);
    ASSERT_FALSE(tt.probe(immediate_null_key).has_value());
    ASSERT_FALSE(tt.probe(descendant_null_key).has_value());

    tt.store_search(workerKey(), real_move, 0, 0, TT_Flag::Lowerbound, workerPly());
    (void)runNonPvAlphaBeta(-50, 50, search_depth, false);

    EXPECT_FALSE(tt.probe(immediate_null_key).has_value())
        << "the immediate null child must still forbid a second null move";
    EXPECT_TRUE(tt.probe(descendant_null_key).has_value())
        << "after a real move from the null child, descendants should regain null-move eligibility";

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(nullMoveTriesAt(0), 0U);
    EXPECT_GT(nullMoveTriesAt(1), 0U);
#endif
}

TEST_F(SearchTest, AlphaBetaRazoringReturnsQsearchFailLowWithoutParentMainTtStore) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;

    TestBoard board{quiet_control};
    loadWorkerBoard(board, search_depth);

    const int static_eval = evaluate(board);
    const int alpha       = static_eval + 901;
    const int beta        = alpha + 100;
    const int value       = runNonPvAlphaBeta(alpha, beta, search_depth);

    EXPECT_EQ(value, static_eval);
    auto record = workerTtRecord();
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->depth, 0);
    EXPECT_NE(record->depth, search_depth);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(razorTriesAt(0), 1U);
    EXPECT_EQ(razorCutoffsAt(0), 1U);
#endif
}

TEST_F(SearchTest, AlphaBetaRazoringRequiresAllGuards) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;

    struct Case {
        const char* name;
        const char* fen;
        int         depth;
        bool        can_null;
        bool        pv;
        bool        seed_tt_move;
        int         alpha_offset;
    };

    const std::array cases{
        Case{"PV node", quiet_control, search_depth, true, true, false, 901},
        Case{"in check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", search_depth, true, false, false, 901},
        Case{"depth above max", quiet_control, 4, true, false, false, 1901},
        Case{"can_null false", STARTFEN, search_depth, false, false, false, 901},
        Case{"TT move available", STARTFEN, search_depth, true, false, true, 901},
        Case{"static eval above margin", STARTFEN, search_depth, true, false, false, 899},
    };

    const auto expectRazoringBlocked = [&](const Case& tc) {
        SCOPED_TRACE(tc.name);

        TestBoard board{tc.fen};
        loadWorkerBoard(board, tc.depth);

        if (tc.seed_tt_move) {
            const Move tt_move = firstWorkerPickerMove();
            ASSERT_FALSE(tt_move.is_null()) << tc.fen;
            tt.store_search(workerKey(), tt_move, 0, 0, TT_Flag::Exact, workerPly());
        }

        const int static_eval = evaluate(board);
        const int alpha       = static_eval + tc.alpha_offset;
        const int beta        = alpha + 100;
        if (tc.pv) {
            PrincipalVariation pv;
            (void)runPvAlphaBeta(alpha, beta, tc.depth, pv);
        } else {
            (void)runNonPvAlphaBeta(alpha, beta, tc.depth, tc.can_null);
        }

        auto record = workerTtRecord();
        ASSERT_TRUE(record.has_value()) << tc.fen;
        EXPECT_EQ(record->depth, tc.depth) << tc.fen;

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(razorTriesAt(0), 0U) << tc.fen;
        EXPECT_EQ(razorCutoffsAt(0), 0U) << tc.fen;
#endif
    };

    for (const auto& tc : cases) {
        expectRazoringBlocked(tc);
    }
}

TEST_F(SearchTest, AlphaBetaFutilitySkipsOnlyAfterFirstLegalQuietMove) {
    constexpr int search_depth = 2;

    TestBoard expected_board{STARTFEN};
    loadWorkerBoard(expected_board, search_depth);

    const int  alpha      = evaluate(expected_board) + 401;
    const int  beta       = alpha + 100;
    const Move first_move = firstWorkerPickerMove();
    ASSERT_FALSE(first_move.is_null());

    const int expected = scoreWorkerChild(first_move, alpha, beta, search_depth - 1);

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);
    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth), expected);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(futilitySkipsAt(0), 1U);
#endif
}

TEST_F(SearchTest, AlphaBetaFutilityRequiresAllGuards) {
    constexpr int search_depth = 2;

    struct Case {
        const char* name;
        const char* fen;
        int         depth;
        int         alpha;
        bool        pv;
    };

    TestBoard static_eval_board{STARTFEN};
    const int static_eval = evaluate(static_eval_board);

    const std::array cases{
        Case{"PV node", STARTFEN, search_depth, static_eval + 401, true},
        Case{"in check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", search_depth, 1000, false},
        Case{"depth above max", STARTFEN, 4, static_eval + 401, false},
        Case{"mate-adjacent alpha", STARTFEN, search_depth, MATE_BOUND, false},
        Case{"static eval above margin", STARTFEN, search_depth, static_eval + 399, false},
    };

    const auto expectFutilityBlocked = [&](const Case& tc) {
        SCOPED_TRACE(tc.name);

        TestBoard board{tc.fen};
        loadWorkerBoard(board, tc.depth);

        const int beta = tc.alpha + 100;
        if (tc.pv) {
            PrincipalVariation pv;
            (void)runPvAlphaBeta(tc.alpha, beta, tc.depth, pv);
        } else {
            (void)runNonPvAlphaBeta(tc.alpha, beta, tc.depth, false);
        }

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(futilitySkipsAt(0), 0U) << tc.fen;
#endif
    };

    for (const auto& tc : cases) {
        expectFutilityBlocked(tc);
    }
}

TEST_F(SearchTest, AlphaBetaFutilityKeepsCapturesPromotionsAndCheckingMoves) {
    constexpr int search_depth = 2;

    struct Case {
        const char* fen;
        Move        first;
        Move        candidate;
        bool        candidate_is_killer;
    };

    const std::array cases{
        Case{"4k3/8/8/8/8/8/R6r/4K3 w - - 0 1", Move(E1, D1), Move(A2, H2), false},
        Case{
            "4k3/P6p/8/8/8/8/8/4K3 w - - 0 1", Move(E1, D1), Move(A7, A8, MOVE_PROM, QUEEN), false},
        Case{"4k3/8/8/8/8/8/R7/4K3 w - - 0 1", Move(A2, A3), Move(A2, E2), true},
    };

    for (const auto& tc : cases) {
        TestBoard board{tc.fen};
        loadWorkerBoard(board, search_depth);
        ASSERT_TRUE(workerLegalGeneratedMove(tc.first)) << tc.fen;
        ASSERT_TRUE(workerLegalGeneratedMove(tc.candidate)) << tc.fen;

        const int alpha       = evaluate(board) + 401;
        const int beta        = alpha + 1000;
        const int child_score = -(beta + 100);

        tt.store_search(workerKey(), tc.first, 0, 0, TT_Flag::Exact, workerPly());
        if (tc.candidate_is_killer)
            seedWorkerKiller(tc.candidate);
        storeWorkerChildSearchTt(tc.candidate, child_score, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth), -child_score) << tc.fen;
    }
}

TEST_F(SearchTest, AlphaBetaQuietCutoffRewardsMoveAndLeavesEarlierQuietUnchanged) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 2U);

    const Move failed_quiet = moves[0];
    const Move cutoff_quiet = moves[1];
    ASSERT_FALSE(board.is_capture(failed_quiet));
    ASSERT_FALSE(board.is_capture(cutoff_quiet));
    ASSERT_NE(failed_quiet.type(), MOVE_PROM);
    ASSERT_NE(cutoff_quiet.type(), MOVE_PROM);

    storeWorkerChildSearchTt(failed_quiet, 0, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_EQ(workerQuietHistory(failed_quiet), 0);
    EXPECT_GT(workerQuietHistory(cutoff_quiet), 0);
}

TEST_F(SearchTest, AlphaBetaQuietCutoffLeavesFailedQuietTtHintUnchanged) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 2U);

    const Move failed_tt_quiet = moves[0];
    const Move cutoff_quiet    = moves[1];
    ASSERT_FALSE(board.is_capture(failed_tt_quiet));
    ASSERT_FALSE(board.is_capture(cutoff_quiet));
    ASSERT_NE(failed_tt_quiet.type(), MOVE_PROM);
    ASSERT_NE(cutoff_quiet.type(), MOVE_PROM);

    tt.store_search(workerKey(), failed_tt_quiet, 0, 0, TT_Flag::Upperbound, workerPly());
    storeWorkerChildSearchTt(failed_tt_quiet, 0, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_EQ(workerQuietHistory(failed_tt_quiet), 0);
    EXPECT_GT(workerQuietHistory(cutoff_quiet), 0);
}

TEST_F(SearchTest, AlphaBetaQuietCutoffStoresCounterForPreviousMove) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const Move previous = findWorkerMove("e2e4");
    Move       cutoff_quiet;

    withWorkerMove(previous, [&] {
        const auto moves = workerPickerLegalMoves();
        const auto it    = std::find_if(moves.begin(), moves.end(), [&](Move move) {
            return !workerIsCapture(move) && move.type() != MOVE_PROM;
        });
        ASSERT_NE(it, moves.end());
        cutoff_quiet = *it;

        storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    });

    EXPECT_EQ(workerCounterMove(WHITE, PAWN, E4), cutoff_quiet);
}

TEST_F(SearchTest, AlphaBetaQuietTtHintCutoffStoresCounterWhenSearched) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const Move previous = findWorkerMove("e2e4");
    Move       cutoff_quiet;

    withWorkerMove(previous, [&] {
        const auto moves = workerPickerLegalMoves();
        const auto it    = std::find_if(moves.begin(), moves.end(), [&](Move move) {
            return !workerIsCapture(move) && move.type() != MOVE_PROM;
        });
        ASSERT_NE(it, moves.end());
        cutoff_quiet = *it;

        tt.store_search(workerKey(), cutoff_quiet, 0, 0, TT_Flag::Upperbound, workerPly());
        storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    });

    EXPECT_EQ(workerCounterMove(WHITE, PAWN, E4), cutoff_quiet);
}

TEST_F(SearchTest, AlphaBetaQuietCutoffRewardsContinuationHistory) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const Move previous = findWorkerMove("e2e4");

    withWorkerMove(previous, [&] {
        const auto moves = workerPickerLegalMoves();
        const auto it    = std::find_if(moves.begin(), moves.end(), [&](Move move) {
            return !workerIsCapture(move) && move.type() != MOVE_PROM;
        });
        ASSERT_NE(it, moves.end());
        const Move cutoff_quiet = *it;

        storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
        EXPECT_GT(workerContinuationHistory(WHITE, PAWN, E4, cutoff_quiet), 0);
    });
}

TEST_F(SearchTest, AlphaBetaQuietCutoffWithoutPreviousMoveDoesNotStoreCounter) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    const auto it    = std::find_if(moves.begin(), moves.end(), [&](Move move) {
        return !workerIsCapture(move) && move.type() != MOVE_PROM;
    });
    ASSERT_NE(it, moves.end());
    const Move      cutoff_quiet = *it;
    const PieceType piece        = workerPieceTypeOn(cutoff_quiet.from());

    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_EQ(workerCounterMove(workerSideToMove(), piece, cutoff_quiet.to()), NULL_MOVE);
}

TEST_F(SearchTest, AlphaBetaQuietCutoffAfterNullMoveDoesNotStoreCounter) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    Move      cutoff_quiet;
    PieceType prev_piece = NO_PIECETYPE;
    Color     prev_c     = WHITE;

    withWorkerNullMove([&] {
        prev_c           = workerSideToMove();
        const auto moves = workerPickerLegalMoves();
        const auto it    = std::find_if(moves.begin(), moves.end(), [&](Move move) {
            return !workerIsCapture(move) && move.type() != MOVE_PROM;
        });
        ASSERT_NE(it, moves.end());
        cutoff_quiet = *it;
        prev_piece   = workerPieceTypeOn(cutoff_quiet.from());

        storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    });

    EXPECT_EQ(workerCounterMove(prev_c, prev_piece, cutoff_quiet.to()), NULL_MOVE);
}

TEST_F(SearchTest, AlphaBetaQuietCutoffWithoutPreviousMoveDoesNotUpdateContinuationHistory) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    const auto it    = std::find_if(moves.begin(), moves.end(), [&](Move move) {
        return !workerIsCapture(move) && move.type() != MOVE_PROM;
    });
    ASSERT_NE(it, moves.end());
    const Move cutoff_quiet = *it;

    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_EQ(workerContinuationHistory(WHITE, PAWN, E4, cutoff_quiet), 0);
}

TEST_F(SearchTest, AlphaBetaQuietCutoffAfterNullMoveDoesNotUpdateContinuationHistory) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    withWorkerNullMove([&] {
        const auto moves = workerPickerLegalMoves();
        const auto it    = std::find_if(moves.begin(), moves.end(), [&](Move move) {
            return !workerIsCapture(move) && move.type() != MOVE_PROM;
        });
        ASSERT_NE(it, moves.end());
        const Move cutoff_quiet = *it;

        storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
        EXPECT_EQ(workerContinuationHistory(WHITE, PAWN, E4, cutoff_quiet), 0);
    });
}

TEST_F(SearchTest, AlphaBetaCaptureCutoffDoesNotStoreCounter) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{"r3k3/B7/8/8/8/8/8/4K3 w - - 0 1"};
    loadWorkerBoard(board, search_depth);

    const Move previous = Move(E1, D1);
    const Move capture  = Move(A8, A7);
    ASSERT_TRUE(workerLegalMove(previous));

    withWorkerMove(previous, [&] {
        ASSERT_TRUE(workerIsCapture(capture));
        ASSERT_TRUE(workerLegalMove(capture));

        tt.store_search(workerKey(), capture, 0, 0, TT_Flag::Upperbound, workerPly());
        storeWorkerChildSearchTt(capture, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    });

    EXPECT_EQ(workerCounterMove(WHITE, KING, D1), NULL_MOVE);
}

TEST_F(SearchTest, AlphaBetaPromotionCutoffDoesNotStoreCounter) {
    constexpr int search_depth = 2;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{"4k3/8/8/8/8/8/p7/4K3 w - - 0 1"};
    loadWorkerBoard(board, search_depth);

    const Move previous  = Move(E1, D1);
    const Move promotion = Move(A2, A1, MOVE_PROM, QUEEN);
    ASSERT_TRUE(workerLegalMove(previous));

    withWorkerMove(previous, [&] {
        ASSERT_EQ(promotion.type(), MOVE_PROM);
        ASSERT_TRUE(workerLegalMove(promotion));

        tt.store_search(workerKey(), promotion, 0, 0, TT_Flag::Upperbound, workerPly());
        storeWorkerChildSearchTt(promotion, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    });

    EXPECT_EQ(workerCounterMove(WHITE, KING, D1), NULL_MOVE);
}

TEST_F(SearchTest, AlphaBetaQuietMalusPenalizesTwoFailedOrdinaryQuiets) {
    constexpr int search_depth = 4;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 3U);

    const Move failed_quiet_1 = moves[0];
    const Move failed_quiet_2 = moves[1];
    const Move cutoff_quiet   = moves[2];
    ASSERT_FALSE(board.is_capture(failed_quiet_1));
    ASSERT_FALSE(board.is_capture(failed_quiet_2));
    ASSERT_FALSE(board.is_capture(cutoff_quiet));
    ASSERT_NE(failed_quiet_1.type(), MOVE_PROM);
    ASSERT_NE(failed_quiet_2.type(), MOVE_PROM);
    ASSERT_NE(cutoff_quiet.type(), MOVE_PROM);

    storeWorkerChildSearchTt(failed_quiet_1, 0, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(failed_quiet_2, 50, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_LT(workerQuietHistory(failed_quiet_1), 0);
    EXPECT_LT(workerQuietHistory(failed_quiet_2), 0);
    EXPECT_GT(workerQuietHistory(cutoff_quiet), 0);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(quietCutoffsAt(search_depth), 1U);
    EXPECT_EQ(quietMalusEligibleNodesAt(search_depth), 1U);
    EXPECT_EQ(quietMalusFailedQuietsAt(search_depth), 2U);
    EXPECT_EQ(quietMalusUpdatesAt(search_depth), 2U);
#endif
}

TEST_F(SearchTest, AlphaBetaQuietMalusRequiresTwoFailedOrdinaryQuiets) {
    constexpr int search_depth = 4;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 2U);

    const Move failed_quiet = moves[0];
    const Move cutoff_quiet = moves[1];
    ASSERT_FALSE(board.is_capture(failed_quiet));
    ASSERT_FALSE(board.is_capture(cutoff_quiet));
    ASSERT_NE(failed_quiet.type(), MOVE_PROM);
    ASSERT_NE(cutoff_quiet.type(), MOVE_PROM);

    storeWorkerChildSearchTt(failed_quiet, 0, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_EQ(workerQuietHistory(failed_quiet), 0);
    EXPECT_GT(workerQuietHistory(cutoff_quiet), 0);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(quietCutoffsAt(search_depth), 1U);
    EXPECT_EQ(quietMalusEligibleNodesAt(search_depth), 1U);
    EXPECT_EQ(quietMalusFailedQuietsAt(search_depth), 1U);
    EXPECT_EQ(quietMalusUpdatesAt(search_depth), 0U);
#endif
}

TEST_F(SearchTest, AlphaBetaQuietMalusSkipsFailedTtQuiet) {
    constexpr int search_depth = 4;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 4U);

    const Move failed_tt_quiet = moves[0];
    const Move failed_quiet_1  = moves[1];
    const Move failed_quiet_2  = moves[2];
    const Move cutoff_quiet    = moves[3];
    ASSERT_FALSE(board.is_capture(failed_tt_quiet));
    ASSERT_FALSE(board.is_capture(failed_quiet_1));
    ASSERT_FALSE(board.is_capture(failed_quiet_2));
    ASSERT_FALSE(board.is_capture(cutoff_quiet));

    tt.store_search(
        workerKey(), failed_tt_quiet, 0, search_depth, TT_Flag::Upperbound, workerPly());
    storeWorkerChildSearchTt(failed_tt_quiet, 0, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(failed_quiet_1, 25, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(failed_quiet_2, 50, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_EQ(workerQuietHistory(failed_tt_quiet), 0);
    EXPECT_LT(workerQuietHistory(failed_quiet_1), 0);
    EXPECT_LT(workerQuietHistory(failed_quiet_2), 0);
    EXPECT_GT(workerQuietHistory(cutoff_quiet), 0);
}

TEST_F(SearchTest, AlphaBetaQuietMalusSkipsFailedKillerQuiet) {
    constexpr int search_depth = 4;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 4U);

    const Move failed_killer  = moves[0];
    const Move failed_quiet_1 = moves[1];
    const Move failed_quiet_2 = moves[2];
    const Move cutoff_quiet   = moves[3];
    ASSERT_FALSE(board.is_capture(failed_killer));
    ASSERT_FALSE(board.is_capture(failed_quiet_1));
    ASSERT_FALSE(board.is_capture(failed_quiet_2));
    ASSERT_FALSE(board.is_capture(cutoff_quiet));

    seedWorkerKiller(failed_killer);
    storeWorkerChildSearchTt(failed_killer, 0, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(failed_quiet_1, 25, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(failed_quiet_2, 50, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_EQ(workerQuietHistory(failed_killer), 0);
    EXPECT_LT(workerQuietHistory(failed_quiet_1), 0);
    EXPECT_LT(workerQuietHistory(failed_quiet_2), 0);
    EXPECT_GT(workerQuietHistory(cutoff_quiet), 0);
}

TEST_F(SearchTest, AlphaBetaQuietMalusRequiresMinimumDepth) {
    constexpr int search_depth = 3;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 3U);

    const Move failed_quiet_1 = moves[0];
    const Move failed_quiet_2 = moves[1];
    const Move cutoff_quiet   = moves[2];
    ASSERT_FALSE(board.is_capture(failed_quiet_1));
    ASSERT_FALSE(board.is_capture(failed_quiet_2));
    ASSERT_FALSE(board.is_capture(cutoff_quiet));

    storeWorkerChildSearchTt(failed_quiet_1, 0, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(failed_quiet_2, 50, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
    EXPECT_EQ(workerQuietHistory(failed_quiet_1), 0);
    EXPECT_EQ(workerQuietHistory(failed_quiet_2), 0);
    EXPECT_GT(workerQuietHistory(cutoff_quiet), 0);
}

TEST_F(SearchTest, AlphaBetaContinuationMalusPenalizesTwoFailedOrdinaryQuiets) {
    constexpr int search_depth = 4;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const Move previous = findWorkerMove("e2e4");

    withWorkerMove(previous, [&] {
        const auto moves = workerPickerLegalMoves();
        ASSERT_GE(moves.size(), 3U);

        const Move failed_quiet_1 = moves[0];
        const Move failed_quiet_2 = moves[1];
        const Move cutoff_quiet   = moves[2];
        ASSERT_FALSE(workerIsCapture(failed_quiet_1));
        ASSERT_FALSE(workerIsCapture(failed_quiet_2));
        ASSERT_FALSE(workerIsCapture(cutoff_quiet));

        storeWorkerChildSearchTt(failed_quiet_1, 0, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(failed_quiet_2, 50, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
        EXPECT_LT(workerContinuationHistory(WHITE, PAWN, E4, failed_quiet_1), 0);
        EXPECT_LT(workerContinuationHistory(WHITE, PAWN, E4, failed_quiet_2), 0);
        EXPECT_GT(workerContinuationHistory(WHITE, PAWN, E4, cutoff_quiet), 0);
    });
}

TEST_F(SearchTest, AlphaBetaContinuationMalusSkipsFailedTtAndKillerQuiets) {
    constexpr int search_depth = 4;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const Move previous = findWorkerMove("e2e4");

    withWorkerMove(previous, [&] {
        const auto moves = workerPickerLegalMoves();
        ASSERT_GE(moves.size(), 5U);

        const Move failed_tt_quiet = moves[0];
        const Move failed_killer   = moves[1];
        const Move failed_quiet_1  = moves[2];
        const Move failed_quiet_2  = moves[3];
        const Move cutoff_quiet    = moves[4];
        ASSERT_FALSE(workerIsCapture(failed_tt_quiet));
        ASSERT_FALSE(workerIsCapture(failed_killer));
        ASSERT_FALSE(workerIsCapture(failed_quiet_1));
        ASSERT_FALSE(workerIsCapture(failed_quiet_2));
        ASSERT_FALSE(workerIsCapture(cutoff_quiet));

        tt.store_search(
            workerKey(), failed_tt_quiet, 0, search_depth, TT_Flag::Upperbound, workerPly());
        seedWorkerKiller(failed_killer);
        storeWorkerChildSearchTt(failed_tt_quiet, 0, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(failed_killer, 10, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(failed_quiet_1, 25, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(failed_quiet_2, 50, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
        EXPECT_EQ(workerContinuationHistory(WHITE, PAWN, E4, failed_tt_quiet), 0);
        EXPECT_EQ(workerContinuationHistory(WHITE, PAWN, E4, failed_killer), 0);
        EXPECT_LT(workerContinuationHistory(WHITE, PAWN, E4, failed_quiet_1), 0);
        EXPECT_LT(workerContinuationHistory(WHITE, PAWN, E4, failed_quiet_2), 0);
        EXPECT_GT(workerContinuationHistory(WHITE, PAWN, E4, cutoff_quiet), 0);
    });
}

TEST_F(SearchTest, AlphaBetaContinuationMalusRequiresMinimumDepth) {
    constexpr int search_depth = 3;
    constexpr int alpha        = -200;
    constexpr int beta         = 100;
    constexpr int cutoff_value = beta + 100;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    const Move previous = findWorkerMove("e2e4");

    withWorkerMove(previous, [&] {
        const auto moves = workerPickerLegalMoves();
        ASSERT_GE(moves.size(), 3U);

        const Move failed_quiet_1 = moves[0];
        const Move failed_quiet_2 = moves[1];
        const Move cutoff_quiet   = moves[2];
        ASSERT_FALSE(workerIsCapture(failed_quiet_1));
        ASSERT_FALSE(workerIsCapture(failed_quiet_2));
        ASSERT_FALSE(workerIsCapture(cutoff_quiet));

        storeWorkerChildSearchTt(failed_quiet_1, 0, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(failed_quiet_2, 50, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(cutoff_quiet, -cutoff_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_value);
        EXPECT_EQ(workerContinuationHistory(WHITE, PAWN, E4, failed_quiet_1), 0);
        EXPECT_EQ(workerContinuationHistory(WHITE, PAWN, E4, failed_quiet_2), 0);
        EXPECT_GT(workerContinuationHistory(WHITE, PAWN, E4, cutoff_quiet), 0);
    });
}

TEST_F(SearchTest, PvAlphaBetaMatchesFullWindowBaselineAndBuildsPv) {
    struct Case {
        const char* fen;
        int         depth;
    };

    const std::array cases{
        Case{STARTFEN, 3},
        Case{POS6, 3},
    };

    for (const auto& tc : cases) {
        TestBoard baseline_board{tc.fen};
        loadWorkerBoard(baseline_board, tc.depth);
        const int baseline = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, tc.depth);

        TestBoard pvs_board{tc.fen};
        loadWorkerBoard(pvs_board, tc.depth);
        PrincipalVariation pv;
        const int          pvs_value = runPvAlphaBeta(-INF_VALUE, INF_VALUE, tc.depth, pv);

        EXPECT_EQ(pvs_value, baseline) << tc.fen;
        ASSERT_FALSE(pv.empty()) << tc.fen;
        EXPECT_TRUE(pvs_board.is_legal_move(pv.front())) << tc.fen;
    }
}

TEST_F(SearchTest, PvNodesIgnoreNonExactMainAndQsearchTtBounds) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;

    TestBoard baseline_board{quiet_control};
    loadWorkerBoard(baseline_board, search_depth);
    PrincipalVariation baseline_pv;
    const int          baseline = runPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth, baseline_pv);

    const int alpha       = baseline - 100;
    const int beta        = baseline + 100;
    const int bogus_score = beta + 25;

    TestBoard non_pv_board{quiet_control};
    loadWorkerBoard(non_pv_board, search_depth);
    tt.store_search(
        workerKey(), NULL_MOVE, bogus_score, search_depth, TT_Flag::Lowerbound, workerPly());
    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth), bogus_score);

    TestBoard pv_board{quiet_control};
    loadWorkerBoard(pv_board, search_depth);
    tt.store_search(
        workerKey(), NULL_MOVE, bogus_score, search_depth, TT_Flag::Lowerbound, workerPly());
    PrincipalVariation pv;
    EXPECT_EQ(runPvAlphaBeta(alpha, beta, search_depth, pv), baseline);

    TestBoard q_baseline_board{quiet_control};
    loadWorkerBoard(q_baseline_board);
    PrincipalVariation q_pv;
    const int          q_baseline = runPvQuiescence(-INF_VALUE, INF_VALUE, q_pv);
    const int          q_alpha    = q_baseline - 100;
    const int          q_beta     = q_baseline + 100;
    const int          q_bogus    = q_beta + 25;

    TestBoard q_non_pv_board{quiet_control};
    loadWorkerBoard(q_non_pv_board);
    tt.store_search(workerKey(), NULL_MOVE, q_bogus, 0, TT_Flag::Lowerbound, workerPly());
    EXPECT_EQ(runQuiescence(q_alpha, q_beta), q_bogus);

    TestBoard q_pv_board{quiet_control};
    loadWorkerBoard(q_pv_board);
    tt.store_search(workerKey(), NULL_MOVE, q_bogus, 0, TT_Flag::Lowerbound, workerPly());
    PrincipalVariation bounded_q_pv;
    EXPECT_EQ(runPvQuiescence(q_alpha, q_beta, bounded_q_pv), q_baseline);
}

TEST_F(SearchTest, PvQuiescencePropagatesPvNodeToChildTtBounds) {
    constexpr auto winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    TestBoard      baseline_board{winning_capture};
    loadWorkerBoard(baseline_board);

    const int static_eval = evaluate(baseline_board);
    const int alpha       = static_eval - 100;
    const int beta        = static_eval + 500;
    const int child_score = -static_eval + 10;

    PrincipalVariation baseline_pv;
    const int          baseline = runPvQuiescence(alpha, beta, baseline_pv);

    TestBoard bounded_board{winning_capture};
    loadWorkerBoard(bounded_board);

    const Move capture = findWorkerMove("d1e2");
    storeQsearchTtAfterMove(capture, child_score, TT_Flag::Lowerbound);

    PrincipalVariation pv;
    EXPECT_EQ(runPvQuiescence(alpha, beta, pv), baseline);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(qTtProbesAt(0), 1U);
    EXPECT_EQ(qTtHitsAt(0), 0U);
    EXPECT_GT(qTtProbesAt(1), 0U);
    EXPECT_GT(qTtHitsAt(1), 0U);
    EXPECT_EQ(qTtCutoffsAt(1), 0U);
#endif
}

TEST_F(SearchTest, RootPvsResearchesLateMoveThatImprovesAlpha) {
    constexpr auto tactic       = "k7/4r3/8/8/8/3Q4/4p3/K7 w - - 0 1";
    constexpr int  search_depth = 1;
    TestBoard      board{tactic};
    loadWorkerBoard(board, search_depth);

    buildRootLines();

    const Move first_move   = findWorkerMove("d3e2");
    const Move winning_move = findWorkerMove("d3d8");
    ASSERT_FALSE(first_move.is_null());
    ASSERT_FALSE(winning_move.is_null());

    RootLine first_line;
    RootLine best_line;
    bool     found_first = false;
    bool     found_best  = false;
    for (const RootLine& line : rootLines()) {
        if (line.root_move == first_move) {
            first_line  = line;
            found_first = true;
        }
        if (line.root_move == winning_move) {
            best_line  = line;
            found_best = true;
        }
    }
    ASSERT_TRUE(found_first);
    ASSERT_TRUE(found_best);

    mutableRootLines() = {first_line, best_line};
    ASSERT_TRUE(runRootWindow(search_depth, -INF_VALUE, INF_VALUE));

    ASSERT_EQ(rootLines().size(), 2U);
    EXPECT_GT(rootLines()[1].value, rootLines()[0].value);

    std::stable_sort(mutableRootLines().begin(), mutableRootLines().end(), is_better_root_line);
    EXPECT_EQ(rootLines().front().root_move, winning_move);
    EXPECT_EQ(count_protocol_lines_starting_with("info depth 1 "), 2) << protocol_out.str();

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(pvsResearchesAt(1), 0U);
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
        EXPECT_FALSE(line.root_move.is_null());
        EXPECT_TRUE(board.is_legal_generated_move(line.root_move));
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
    EXPECT_EQ(rootLines().front().root_move, rootMove());
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

TEST_F(SearchTest, SearchReportingSuppressesOnlyIdenticalRootLines) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 2);

    RootLine line{
        .root_move = Move(E2, E4),
        .value     = 20,
        .depth     = 1,
        .completed = true,
        .pv        = pv_for_move(Move(E2, E4)),
    };

    reportRootProgress(line);
    reportRootProgress(line);
    EXPECT_EQ(count_protocol_lines_starting_with("info depth 1 "), 1) << protocol_out.str();

    RootLine score_changed = line;
    score_changed.value    = 21;
    reportRootProgress(score_changed);
    EXPECT_EQ(count_protocol_lines_starting_with("info depth 1 "), 2) << protocol_out.str();

    RootLine pv_changed = score_changed;
    pv_changed.pv       = pv_for_line(Move(E2, E4), Move(E7, E5));
    reportRootProgress(pv_changed);
    EXPECT_EQ(count_protocol_lines_starting_with("info depth 1 "), 3) << protocol_out.str();

    RootLine depth_changed = pv_changed;
    depth_changed.depth    = 2;
    reportRootProgress(depth_changed);
    EXPECT_EQ(count_protocol_lines_starting_with("info depth 2 "), 1) << protocol_out.str();
}

TEST_F(SearchTest, RootAspirationWidensAfterFailHighAndCompletes) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 1);

    ASSERT_TRUE(runRootDepth(1, -1000));

    EXPECT_TRUE(rootCompleted());
    EXPECT_TRUE(rootMoveIsLegal());
    EXPECT_EQ(rootDepth(), 1);
    EXPECT_GT(rootValue(), -950);
    ASSERT_FALSE(rootPvEmpty());
    EXPECT_EQ(rootPvFront(), rootMove());

    const std::string transcript = protocol_out.str();
    EXPECT_GE(count_protocol_lines_starting_with("info depth 1 "), 1) << transcript;

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(aspirationFailHighs(), 0U);
    EXPECT_EQ(aspirationFailLows(), 0U);
    EXPECT_EQ(aspirationResearches(), aspirationFailHighs());
#endif
}

TEST_F(SearchTest, RootAspirationWidensAfterFailLowAndCompletes) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 1);

    ASSERT_TRUE(runRootDepth(1, 1000));

    EXPECT_TRUE(rootCompleted());
    EXPECT_TRUE(rootMoveIsLegal());
    EXPECT_EQ(rootDepth(), 1);
    EXPECT_LT(rootValue(), 950);
    ASSERT_FALSE(rootPvEmpty());
    EXPECT_EQ(rootPvFront(), rootMove());

    const std::string transcript = protocol_out.str();
    EXPECT_GE(count_protocol_lines_starting_with("info depth 1 "), 1) << transcript;

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(aspirationFailLows(), 0U);
    EXPECT_EQ(aspirationFailHighs(), 0U);
    EXPECT_EQ(aspirationResearches(), aspirationFailLows());
#endif
}

TEST_F(SearchTest, StoppedRootAspirationPreservesLastAcceptedRootSnapshot) {
    TestBoard board{STARTFEN};
    loadWorkerBoard(board, 2);

    ASSERT_TRUE(runRootDepth(1, evaluate(board)));
    const RootLine accepted = rootSnapshot();
    ASSERT_TRUE(accepted.usable_root_move());
    ASSERT_EQ(accepted.depth, 1);

    ThreadTestAccess::request_stop(*thread);

    EXPECT_FALSE(runRootDepth(2, accepted.value));

    const RootLine snapshot = rootSnapshot();
    EXPECT_EQ(snapshot.root_move, accepted.root_move);
    EXPECT_EQ(snapshot.value, accepted.value);
    EXPECT_EQ(snapshot.depth, accepted.depth);
    EXPECT_EQ(snapshot.completed, accepted.completed);
    EXPECT_EQ(snapshot.pv.size(), accepted.pv.size());
    ASSERT_FALSE(snapshot.pv.empty());
    EXPECT_EQ(snapshot.pv.front(), accepted.root_move);

    const std::string transcript = protocol_out.str();
    EXPECT_GE(count_protocol_lines_starting_with("info depth 1 "), 1) << transcript;
    EXPECT_EQ(count_protocol_lines_starting_with("info depth 2 "), 0) << transcript;
}

TEST_F(SearchTest, AlphaBetaLmrResearchesFullDepthWhenReducedSearchImprovesAlpha) {
    constexpr int search_depth         = 4;
    constexpr int alpha                = -2000;
    constexpr int beta                 = 2000;
    constexpr int reduced_parent_value = 1500;

    TestBoard baseline_board{STARTFEN};
    loadWorkerBoard(baseline_board, search_depth);
    const int expected = runNonPvAlphaBeta(alpha, beta, search_depth, false);

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);
    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 3U);
    const Move reduced_move = moves[2];

    storeWorkerChildSearchTt(reduced_move, -reduced_parent_value, search_depth - 2, TT_Flag::Exact);

    const int actual = runNonPvAlphaBeta(alpha, beta, search_depth, false);
    EXPECT_EQ(actual, expected);
    EXPECT_NE(actual, reduced_parent_value);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(lmrTriesAt(0), 0U);
    EXPECT_GT(lmrResearchesAt(0), 0U);
#endif
}

TEST_F(SearchTest, AlphaBetaLmrRequiresMinimumDepth) {
    constexpr int search_depth = 2;

    TestBoard board{STARTFEN};
    loadWorkerBoard(board, search_depth);

    (void)runNonPvAlphaBeta(-2000, 2000, search_depth, false);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(lmrTriesAt(0), 0U);
    EXPECT_EQ(lmrResearchesAt(0), 0U);
#endif
}

TEST_F(SearchTest, AlphaBetaLmrRequiresThirdLegalMove) {
    constexpr auto two_legal_moves = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth    = 4;

    TestBoard board{two_legal_moves};
    loadWorkerBoard(board, search_depth);
    ASSERT_EQ(workerPickerLegalMoves().size(), 2U);

    (void)runNonPvAlphaBeta(-2000, 2000, search_depth, false);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(lmrTriesAt(0), 0U);
    EXPECT_EQ(lmrResearchesAt(0), 0U);
#endif
}

TEST_F(SearchTest, AlphaBetaLmrSkipsTacticalVetoMoves) {
    constexpr int search_depth        = 4;
    constexpr int alpha               = -2000;
    constexpr int beta                = -1000;
    constexpr int low_parent_value    = alpha - 100;
    constexpr int cutoff_parent_value = beta + 100;

    struct Case {
        const char* fen;
        Move        tt_move;
        Move        killer_move;
        Move        candidate;
    };

    const std::array cases{
        Case{"4k3/P7/8/8/8/8/8/4K3 w - - 0 1",
             Move(E1, D1),
             NULL_MOVE,
             Move(A7, A8, MOVE_PROM, ROOK)},
        Case{"4k3/8/8/8/6N1/8/8/RB1QK3 w - - 0 1", Move(A1, A8), Move(B1, G6), Move(D1, A4)},
    };

    for (const auto& tc : cases) {
        TestBoard board{tc.fen};
        loadWorkerBoard(board, search_depth);
        ASSERT_TRUE(workerLegalGeneratedMove(tc.tt_move)) << tc.fen;
        ASSERT_TRUE(workerLegalGeneratedMove(tc.candidate)) << tc.fen;
        ASSERT_TRUE(board.is_checking_move(tc.candidate) || tc.candidate.type() == MOVE_PROM)
            << tc.fen;

        tt.store_search(workerKey(), tc.tt_move, 0, 0, TT_Flag::Exact, workerPly());
        if (!tc.killer_move.is_null()) {
            ASSERT_TRUE(workerLegalGeneratedMove(tc.killer_move)) << tc.fen;
            ASSERT_TRUE(board.is_checking_move(tc.killer_move)) << tc.fen;
            seedWorkerKiller(tc.killer_move);
        }
        boostWorkerHistory(tc.candidate, search_depth);

        const auto moves        = workerPickerLegalMoves(tc.tt_move);
        const auto candidate_it = std::find(moves.begin(), moves.end(), tc.candidate);
        ASSERT_NE(candidate_it, moves.end()) << tc.fen;
        ASSERT_EQ(std::distance(moves.begin(), candidate_it), 2) << tc.fen;

        for (auto it = moves.begin(); it != candidate_it; ++it)
            storeWorkerChildSearchTt(*it, -low_parent_value, search_depth - 1, TT_Flag::Exact);
        storeWorkerChildSearchTt(
            tc.candidate, -cutoff_parent_value, search_depth - 1, TT_Flag::Exact);

        EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_parent_value)
            << tc.fen;

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(lmrTriesAt(0), 0U) << tc.fen;
        EXPECT_EQ(lmrResearchesAt(0), 0U) << tc.fen;
#endif
    }
}

TEST_F(SearchTest, AlphaBetaLmrSkipsCheckEvasions) {
    constexpr auto in_check            = "4k3/8/8/8/8/8/4Q3/4K3 b - - 0 1";
    constexpr int  search_depth        = 4;
    constexpr int  alpha               = -2000;
    constexpr int  beta                = -1000;
    constexpr int  low_parent_value    = alpha - 100;
    constexpr int  cutoff_parent_value = beta + 100;

    TestBoard board{in_check};
    loadWorkerBoard(board, search_depth);
    ASSERT_TRUE(board.is_check()) << in_check;

    const auto moves = workerPickerLegalMoves();
    ASSERT_GE(moves.size(), 3U) << in_check;
    const Move candidate = moves[2];

    for (auto it = moves.begin(); it != moves.begin() + 2; ++it)
        storeWorkerChildSearchTt(*it, -low_parent_value, search_depth - 1, TT_Flag::Exact);
    storeWorkerChildSearchTt(candidate, -cutoff_parent_value, search_depth - 1, TT_Flag::Exact);

    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, search_depth, false), cutoff_parent_value);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(lmrTriesAt(0), 0U);
    EXPECT_EQ(lmrResearchesAt(0), 0U);
#endif
}

TEST_F(SearchTest, RootSearchSetsNullMoveForCheckmate) {
    constexpr auto checkmate = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1";
    TestBoard      board{checkmate};
    loadWorkerBoard(board);

    EXPECT_EQ(runRootSearch(), -MATE_VALUE);
    EXPECT_EQ(rootMove(), NULL_MOVE);
    EXPECT_EQ(rootValue(), -MATE_VALUE);
    EXPECT_EQ(rootDepth(), limits.depth);
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
    EXPECT_EQ(rootDepth(), limits.depth);
    EXPECT_TRUE(rootCompleted());
    EXPECT_TRUE(rootLines().empty());
}

TEST_F(SearchTest, CompletedSearchPublishesRootLineSnapshot) {
    constexpr auto mate_in_one = "7R/8/8/8/8/1K6/8/1k6 w - - 0 1";
    TestBoard      board{mate_in_one};
    limits.depth = 2;

    ThreadTestAccess::start_search(*thread, board, limits);
    ThreadTestAccess::wait_for_idle(*thread);

    const auto snapshot = rootSnapshot();
    EXPECT_TRUE(snapshot.completed);
    EXPECT_TRUE(snapshot.has_completed_depth());
    EXPECT_TRUE(snapshot.usable_root_move());
    EXPECT_EQ(snapshot.root_move.str(), "h8h1");
    ASSERT_FALSE(snapshot.pv.empty());
    EXPECT_EQ(snapshot.pv.front(), snapshot.root_move);
    EXPECT_EQ(snapshot.pv.size(), 1);
    EXPECT_EQ(snapshot.value, MATE_VALUE - 1);
    EXPECT_EQ(snapshot.depth, 2);
    EXPECT_GT(workerNodes(), 0U);
}

TEST_F(SearchTest, CompletedNonTerminalSearchPublishesFullRootPv) {
    TestBoard board{STARTFEN};
    limits.depth = 2;

    ThreadTestAccess::start_search(*thread, board, limits);
    ThreadTestAccess::wait_for_idle(*thread);

    const auto snapshot = rootSnapshot();
    ASSERT_TRUE(snapshot.completed);
    ASSERT_TRUE(snapshot.usable_root_move());
    ASSERT_EQ(snapshot.pv.size(), 2);
    EXPECT_EQ(snapshot.pv.front(), snapshot.root_move);
}

TEST_F(SearchTest, CompletedNoLegalMoveSearchPublishesCompletedNullMove) {
    constexpr auto checkmate = "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1";
    TestBoard      board{checkmate};
    limits.depth = 1;

    ThreadTestAccess::start_search(*thread, board, limits);
    ThreadTestAccess::wait_for_idle(*thread);

    const auto snapshot = rootSnapshot();
    EXPECT_TRUE(snapshot.completed);
    EXPECT_TRUE(snapshot.has_completed_depth());
    EXPECT_FALSE(snapshot.usable_root_move());
    EXPECT_EQ(snapshot.root_move, NULL_MOVE);
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
    EXPECT_EQ(snapshot.root_move, NULL_MOVE);
    EXPECT_EQ(snapshot.depth, 0);
    EXPECT_FALSE(snapshot.has_completed_depth());
    EXPECT_FALSE(snapshot.usable_root_move());
    EXPECT_TRUE(snapshot.pv.empty());
}

TEST_F(SearchTest, StoppedSearchPreservesLastCompletedRootLine) {
    TestBoard board{STARTFEN};
    limits.depth = 8;
    limits.nodes = 100;

    ThreadTestAccess::start_search(*thread, board, limits);
    ThreadTestAccess::wait_for_idle(*thread);

    const auto snapshot = rootSnapshot();
    ASSERT_TRUE(snapshot.has_completed_depth());
    EXPECT_TRUE(snapshot.usable_root_move());
    EXPECT_LT(snapshot.depth, limits.depth);
    ASSERT_FALSE(snapshot.pv.empty());
    EXPECT_EQ(snapshot.pv.front(), snapshot.root_move);
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
    const RootLine fallback{.root_move = Move(E2, E4), .value = 50, .depth = 2, .completed = true};
    const std::array<RootLine, 2> lines{
        RootLine{.root_move = Move(G1, F3), .value = 0, .depth = 3, .completed = true},
        RootLine{.root_move = Move(D2, D4), .value = 200, .depth = 1, .completed = true},
    };

    const RootLine selected = select_best_root_line(fallback, lines);

    EXPECT_EQ(selected.root_move, Move(G1, F3));
    EXPECT_EQ(selected.depth, 3);
}

TEST_F(SearchTest, SelectBestRootLineTieBreaksByHigherValue) {
    const RootLine fallback{.root_move = Move(E2, E4), .value = 10, .depth = 3, .completed = true};
    const std::array<RootLine, 2> lines{
        RootLine{.root_move = Move(G1, F3), .value = 25, .depth = 3, .completed = true},
        RootLine{.root_move = Move(D2, D4), .value = 20, .depth = 3, .completed = true},
    };

    const RootLine selected = select_best_root_line(fallback, lines);

    EXPECT_EQ(selected.root_move, Move(G1, F3));
    EXPECT_EQ(selected.value, 25);
}

TEST_F(SearchTest, SelectBestRootLineTieBreaksByLowerMoveBits) {
    const RootLine fallback{.root_move = Move(H2, H3), .value = 10, .depth = 3, .completed = true};
    const RootLine first{.root_move = Move(G2, G3), .value = 10, .depth = 3, .completed = true};
    const RootLine second{.root_move = Move(A2, A3), .value = 10, .depth = 3, .completed = true};
    const std::array<RootLine, 2> lines{first, second};

    const RootLine selected = select_best_root_line(fallback, lines);
    const Move     expected =
        first.root_move.bits < second.root_move.bits ? first.root_move : second.root_move;

    EXPECT_LT(expected.bits, fallback.root_move.bits);
    EXPECT_EQ(selected.root_move, expected);
}

TEST_F(SearchTest, SelectBestRootLineIgnoresUnusableLines) {
    const RootLine fallback{.root_move = Move(E2, E4), .value = 10, .depth = 2, .completed = true};
    const std::array<RootLine, 3> lines{
        RootLine{.root_move = Move(G1, F3), .value = 1000, .depth = 99, .completed = false},
        RootLine{.root_move = NULL_MOVE, .value = 1000, .depth = 99, .completed = true},
        RootLine{.root_move = Move(D2, D4), .value = 1000, .depth = 0, .completed = true},
    };

    const RootLine selected = select_best_root_line(fallback, lines);

    EXPECT_EQ(selected.root_move, fallback.root_move);
    EXPECT_EQ(selected.value, fallback.value);
    EXPECT_EQ(selected.depth, fallback.depth);
}

TEST_F(SearchTest, SelectBestRootLineReturnsFallbackWhenNoUsableLineExists) {
    const RootLine fallback{
        .root_move = NULL_MOVE, .value = DRAW_VALUE, .depth = 1, .completed = true};
    const std::array<RootLine, 2> lines{
        RootLine{.root_move = Move(G1, F3), .value = 1000, .depth = 2, .completed = false},
        RootLine{.root_move = NULL_MOVE, .value = -MATE_VALUE, .depth = 2, .completed = true},
    };

    const RootLine selected = select_best_root_line(fallback, lines);

    EXPECT_TRUE(selected.root_move.is_null());
    EXPECT_TRUE(selected.completed);
    EXPECT_TRUE(selected.has_completed_depth());
    EXPECT_FALSE(selected.usable_root_move());
    EXPECT_EQ(selected.value, DRAW_VALUE);
}
