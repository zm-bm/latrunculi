
#include <algorithm>

#include <gtest/gtest.h>

#include "board.hpp"
#include "evaluator.hpp"

#include "search_options.hpp"
#include "test_util.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "tt.hpp"
#include "uci.hpp"

int                depth    = 8;
int                movetime = 2000;
std::ostringstream oss;
constexpr auto     AnyMove = "ANY";

class SearchTest : public ::testing::Test {
protected:
    uci::Protocol protocol{std::cout, std::cerr};
    ThreadPool    pool{1, protocol};
    Thread*       thread;
    SearchOptions options;
    KillerMoves   sort_killers;
    HistoryTable  sort_history;
    int           sort_ply = 5;

    void SetUp() override {
        thread        = pool.threads[0].get();
        options       = SearchOptions();
        options.depth = depth;
        // options.movetime = movetime;
    }

    int testQuiescence(const std::string& fen, int alpha = -INF_VALUE, int beta = INF_VALUE) {
        TestBoard board{fen};
        tt.clear();
        options.board = &board;
        thread->set_options(options);
        thread->reset();
        return thread->quiescence(alpha, beta);
    }

    int testQuiescenceAfterMove(const std::string& fen,
                                const std::string& move_str,
                                int                alpha = -INF_VALUE,
                                int                beta  = INF_VALUE) {
        TestBoard board{fen};
        tt.clear();
        options.board = &board;
        thread->set_options(options);
        thread->reset();

        auto movelist = generate<ALL_MOVES>(thread->board);
        auto move_it  = std::find_if(movelist.begin(), movelist.end(), [&](const Move& move) {
            return move.str() == move_str && thread->board.is_legal_pseudo_move(move);
        });
        EXPECT_NE(move_it, movelist.end()) << fen;
        if (move_it == movelist.end())
            return 0;

        thread->board.make(*move_it, thread->position_states[thread->ply + 1]);
        ++thread->ply;
        int score = thread->quiescence(alpha, beta);
        thread->board.unmake(thread->position_states[thread->ply - 1]);
        --thread->ply;
        return score;
    }

    int testAlphaBetaAfterMove(const std::string& fen,
                               const std::string& move_str,
                               int                alpha,
                               int                beta,
                               int                search_depth,
                               bool               can_null = true) {
        TestBoard board{fen};
        tt.clear();
        options.board = &board;
        thread->set_options(options);
        thread->reset();

        auto movelist = generate<ALL_MOVES>(thread->board);
        auto move_it  = std::find_if(movelist.begin(), movelist.end(), [&](const Move& move) {
            return move.str() == move_str && thread->board.is_legal_pseudo_move(move);
        });
        EXPECT_NE(move_it, movelist.end()) << fen;
        if (move_it == movelist.end())
            return 0;

        thread->board.make(*move_it, thread->position_states[thread->ply + 1]);
        ++thread->ply;
        int score = thread->alphabeta<NON_PV>(alpha, beta, search_depth, can_null);
        thread->board.unmake(thread->position_states[thread->ply - 1]);
        --thread->ply;
        return score;
    }

    std::pair<int, uint64_t>
    testQuiescenceWithNodes(const std::string& fen, int alpha = -INF_VALUE, int beta = INF_VALUE) {
        TestBoard board{fen};
        tt.clear();
        options.board = &board;
        thread->set_options(options);
        thread->reset();
        return {thread->quiescence(alpha, beta), thread->nodes};
    }

    Move firstLegalPickedMove(Board& board,
                              Move   pv_move   = NULL_MOVE,
                              Move   tt_move   = NULL_MOVE,
                              bool   root      = false,
                              int    thread_id = 0) {
        MoveList         movelist = generate<ALL_MOVES>(board);
        StagedMovePicker picker{
            {board, sort_killers, sort_history, sort_ply, pv_move, tt_move, root, thread_id},
            movelist};

        while (Move* move = picker.next()) {
            if (board.is_legal_pseudo_move(*move))
                return *move;
        }
        return NULL_MOVE;
    }

    std::vector<Move> pickedLegalQuiescenceMoves(Board& board) {
        const bool in_check = board.is_check();
        MoveList   movelist = in_check ? generate<EVASIONS>(board) : generate<CAPTURES>(board);
        QuiescenceMovePicker picker{
            {board, sort_killers, sort_history, sort_ply}, movelist, in_check};

        std::vector<Move> moves;
        while (Move* move = picker.next()) {
            if (board.is_legal_pseudo_move(*move))
                moves.push_back(*move);
        }
        return moves;
    }

    void loadThreadBoard(Board& board) {
        tt.clear();
        options.board = &board;
        thread->set_options(options);
        thread->reset();
    }

    Move findThreadMove(const std::string& move_str) {
        auto movelist = generate<ALL_MOVES>(thread->board);
        auto move_it  = std::find_if(movelist.begin(), movelist.end(), [&](const Move& move) {
            return move.str() == move_str && thread->board.is_legal_pseudo_move(move);
        });
        EXPECT_NE(move_it, movelist.end()) << thread->board.toFEN();
        return move_it == movelist.end() ? NULL_MOVE : *move_it;
    }

    bool threadInCheck() const { return thread->board.is_check(); }

    int threadNonPawnMaterial() const {
        return thread->board.nonPawnMaterial(thread->board.side_to_move());
    }

    uint64_t threadKey() const { return thread->board.key(); }

    uint64_t threadNullChildKey() const {
        TestBoard board_copy{thread->board.toFEN()};
        board_copy.make_null();
        return board_copy.key();
    }

    bool threadDescendantInCheck(Move move) const {
        TestBoard board_copy{thread->board.toFEN()};
        board_copy.make(move);
        return board_copy.is_check();
    }

    int threadDescendantNonPawnMaterial(Move move) const {
        TestBoard board_copy{thread->board.toFEN()};
        board_copy.make(move);
        return board_copy.nonPawnMaterial(board_copy.side_to_move());
    }

    uint64_t threadDescendantNullKey(Move move) const {
        TestBoard board_copy{thread->board.toFEN()};
        board_copy.make(move);
        board_copy.make_null();
        return board_copy.key();
    }

    int runNonPvAlphaBeta(int alpha, int beta, int search_depth, bool can_null) {
        return thread->alphabeta<NON_PV>(alpha, beta, search_depth, can_null);
    }

    int runRootSearchWiden(int search_depth, int prior_value) {
        return thread->search_widen(search_depth, prior_value);
    }

    int runLoadedQuiescence(int alpha, int beta) { return thread->quiescence(alpha, beta); }

    int runLoadedPvQuiescence(int alpha, int beta) { return thread->quiescence<PV>(alpha, beta); }

#if LATRUNCULI_SEARCH_STATS
    void resetThreadStats() { thread->stats.reset(); }

    uint64_t qTtProbes(int search_ply) const { return thread->stats.q_tt_probes[search_ply]; }
    uint64_t qTtHits(int search_ply) const { return thread->stats.q_tt_hits[search_ply]; }
    uint64_t qTtCutoffs(int search_ply) const { return thread->stats.q_tt_cutoffs[search_ply]; }
#endif

    void setThreadPly(int search_ply) { thread->ply = search_ply; }

    std::string currentPv(int pv_depth) const { return thread->get_pv(pv_depth); }

    bool threadMoveIsPseudoLegal(Move move) const { return thread->board.is_pseudo_legal(move); }

    bool threadMoveIsLegal(Move move) const { return thread->board.is_legal_move(move); }

    void testSearch(const std::string fen, int score, std::string move) {
        TestBoard board{fen};
        options.board = &board;
        thread->start(options);
        thread->wait();

        EXPECT_EQ(thread->root_value, score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(thread->root_move.str(), move) << fen;
        }
    }

    void testSearchGT(const std::string fen, int score, std::string move) {
        TestBoard board{fen};
        options.board = &board;
        thread->start(options);
        thread->wait();

        EXPECT_GT(thread->root_value, score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(thread->root_move.str(), move) << fen;
        }
    }
};

TEST_F(SearchTest, basicMates) {
    auto searchpos1 = "7R/8/8/8/8/1K6/8/1k6 w - - 0 1";
    auto searchpos2 = "5rk1/pb2npp1/1pq4p/5p2/5B2/1B6/P2RQ1PP/2r1R2K b - - 0 2";
    auto searchpos3 = "5rk1/pb2npp1/1p5p/5p2/5B2/1B6/P2RQ1qP/2r1R2K w - - 0 3";
    auto searchpos4 = "5rk1/pb2npp1/1p5p/5p2/5B2/1B6/P2R2QP/2r1R2K b - - 0 4";

    std::vector<std::tuple<std::string, int, std::string>> test_cases = {
        {searchpos1, +(MATE_VALUE - 1), "h8h1"},
        {searchpos2, +(MATE_VALUE - 3), "c6g2"},
        {searchpos3, -(MATE_VALUE - 2), "e2g2"},
        {searchpos4, +(MATE_VALUE - 1), "c1e1"},
    };

    for (auto& [fen, expectedScore, expectedMove] : test_cases) {
        testSearch(fen, expectedScore, expectedMove);
    }
}

TEST_F(SearchTest, basicDraws) {
    auto searchpos1 = "r7/5kPK/7P/8/8/8/8/8 b - -";
    auto searchpos2 = "1r6/5kPK/7P/8/8/8/8/8 w - -";
    auto searchpos3 = "1r4Q1/5k1K/7P/8/8/8/8/8 b - -";

    std::vector<std::tuple<std::string, int, std::string>> test_cases = {
        {searchpos1, DRAW_VALUE, AnyMove},
        {searchpos2, DRAW_VALUE, "g7g8q"},
        {searchpos3, DRAW_VALUE, "b8g8"},
    };

    for (auto& [fen, expectedScore, expectedMove] : test_cases) {
        testSearch(fen, expectedScore, expectedMove);
    }
}

TEST_F(SearchTest, QuiescenceInCheckSearchesForcedQuietEvasion) {
    auto      in_check_with_one_quiet_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    TestBoard board{in_check_with_one_quiet_evasion};

    ASSERT_TRUE(board.is_check()) << in_check_with_one_quiet_evasion;

    int actual   = testQuiescence(in_check_with_one_quiet_evasion);
    int expected = -testQuiescenceAfterMove(in_check_with_one_quiet_evasion, "a8b8");

    EXPECT_EQ(actual, expected) << in_check_with_one_quiet_evasion;
    EXPECT_NE(actual, evaluate(board)) << "in-check qsearch must not stand pat";
}

TEST_F(SearchTest, QuiescenceOutOfCheckUsesStandPatInsteadOfSearchingQuiets) {
    auto      quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard board{quiet_control};

    ASSERT_FALSE(board.is_check()) << quiet_control;

    EXPECT_EQ(testQuiescence(quiet_control), evaluate(board)) << quiet_control;
}

TEST_F(SearchTest, FirstLegalPickedMoveSkipsIllegalHashMove) {
    TestBoard board{POS3};
    Move      illegal_hash_move = Move(B5, B6);

    ASSERT_FALSE(board.is_legal_move(illegal_hash_move));

    EXPECT_EQ(firstLegalPickedMove(board, NULL_MOVE, illegal_hash_move), Move(B4, F4));
}

TEST_F(SearchTest, QuiescenceOutOfCheckIncludesPromotions) {
    constexpr auto promotion_fen = "7k/P7/8/8/8/8/8/4K3 w - - 0 1";
    TestBoard      board{promotion_fen};

    ASSERT_FALSE(board.is_check()) << promotion_fen;

    const auto legal_moves = pickedLegalQuiescenceMoves(board);
    ASSERT_EQ(legal_moves.size(), 4U) << promotion_fen;
    EXPECT_EQ(legal_moves[0].str(), "a7a8q");
    EXPECT_EQ(legal_moves[1].str(), "a7a8r");
    EXPECT_EQ(legal_moves[2].str(), "a7a8b");
    EXPECT_EQ(legal_moves[3].str(), "a7a8n");

    const int actual   = testQuiescence(promotion_fen);
    const int expected = std::max({-testQuiescenceAfterMove(promotion_fen, "a7a8q"),
                                   -testQuiescenceAfterMove(promotion_fen, "a7a8r"),
                                   -testQuiescenceAfterMove(promotion_fen, "a7a8b"),
                                   -testQuiescenceAfterMove(promotion_fen, "a7a8n")});

    EXPECT_EQ(actual, expected) << promotion_fen;
}

TEST_F(SearchTest, QuiescenceOutOfCheckSkipsWeakCaptures) {
    constexpr auto weak_capture_fen = "2b3k1/3p4/8/8/8/8/8/3Q2K1 w - - 0 1";
    TestBoard      board{weak_capture_fen};

    loadThreadBoard(board);
    ASSERT_FALSE(threadInCheck()) << weak_capture_fen;

    const auto legal_captures = pickedLegalQuiescenceMoves(board);
    ASSERT_EQ(legal_captures.size(), 1U) << weak_capture_fen;
    EXPECT_EQ(legal_captures[0].str(), "d1d7");
    EXPECT_EQ(legal_captures[0].priority, PRIORITY_WEAK);

    const int stand_pat       = evaluate(board);
    const auto [score, nodes] = testQuiescenceWithNodes(weak_capture_fen);

    EXPECT_EQ(score, stand_pat);
    EXPECT_EQ(nodes, 1U) << "weak captures should be pruned before recursive qsearch";
}

TEST_F(SearchTest, QuiescenceFailLowReturnsStandPatInsteadOfAlpha) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};

    const int     stand_pat = evaluate(board);
    constexpr int alpha     = 100;
    constexpr int beta      = 200;

    ASSERT_LT(stand_pat, alpha) << quiet_control;
    EXPECT_EQ(testQuiescence(quiet_control, alpha, beta), stand_pat) << quiet_control;
}

TEST_F(SearchTest, QuiescenceFailLowInCheckReturnsBestEvasionInsteadOfAlpha) {
    constexpr auto in_check_with_one_quiet_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  alpha                           = 100;
    constexpr int  beta                            = 200;

    const int expected =
        -testQuiescenceAfterMove(in_check_with_one_quiet_evasion, "a8b8", -beta, -alpha);

    ASSERT_LT(expected, alpha) << in_check_with_one_quiet_evasion;
    EXPECT_EQ(testQuiescence(in_check_with_one_quiet_evasion, alpha, beta), expected)
        << in_check_with_one_quiet_evasion;
}

TEST_F(SearchTest, QuiescenceFailHighReturnsRawScoreInsteadOfBeta) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      quiet_board{quiet_control};
    const int      stand_pat = evaluate(quiet_board);

    EXPECT_EQ(testQuiescence(quiet_control, stand_pat - 100, stand_pat - 50), stand_pat)
        << "stand-pat fail-high should return the raw stand-pat score";

    constexpr auto winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    TestBoard      capture_board{winning_capture};
    const int      capture_stand_pat = evaluate(capture_board);
    const int      alpha             = capture_stand_pat - 50;
    const int      beta              = capture_stand_pat + 50;
    const int      capture_score = -testQuiescenceAfterMove(winning_capture, "d1e2", -beta, -alpha);

    ASSERT_GT(capture_score, beta) << winning_capture;
    EXPECT_EQ(testQuiescence(winning_capture, alpha, beta), capture_score)
        << "capture fail-high should return the raw capture score";
}

TEST_F(SearchTest, QuiescenceTranspositionTableExactCutoffReturnsStoredScore) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadThreadBoard(board);

    const int stored_score = evaluate(board) + 500;
    tt.store(threadKey(), NULL_MOVE, stored_score, 0, TT_Flag::Exact, 0);

    EXPECT_EQ(runLoadedQuiescence(-INF_VALUE, INF_VALUE), stored_score);
}

TEST_F(SearchTest, QuiescenceTranspositionTableLowerboundCutsOnlyAtBeta) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    const int      stand_pat = evaluate(board);
    const int      alpha     = stand_pat - 100;
    const int      beta      = stand_pat + 100;

    loadThreadBoard(board);
    tt.store(threadKey(), NULL_MOVE, beta + 25, 0, TT_Flag::Lowerbound, 0);
    EXPECT_EQ(runLoadedQuiescence(alpha, beta), beta + 25)
        << "lowerbound qsearch TT cutoffs should return the raw stored score";

    TestBoard no_cutoff_board{quiet_control};
    loadThreadBoard(no_cutoff_board);
    tt.store(threadKey(), NULL_MOVE, beta - 1, 0, TT_Flag::Lowerbound, 0);
    EXPECT_EQ(runLoadedQuiescence(alpha, beta), stand_pat)
        << "lowerbound qsearch TT entries below beta must not replace search";
}

TEST_F(SearchTest, QuiescenceTranspositionTableUpperboundCutsOnlyAtAlpha) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    const int      stand_pat = evaluate(board);
    const int      alpha     = stand_pat - 100;
    const int      beta      = stand_pat + 100;

    loadThreadBoard(board);
    tt.store(threadKey(), NULL_MOVE, alpha - 25, 0, TT_Flag::Upperbound, 0);
    EXPECT_EQ(runLoadedQuiescence(alpha, beta), alpha - 25)
        << "upperbound qsearch TT cutoffs should return the raw stored score";

    TestBoard no_cutoff_board{quiet_control};
    loadThreadBoard(no_cutoff_board);
    tt.store(threadKey(), NULL_MOVE, alpha + 1, 0, TT_Flag::Upperbound, 0);
    EXPECT_EQ(runLoadedQuiescence(alpha, beta), stand_pat)
        << "upperbound qsearch TT entries above alpha must not replace search";
}

TEST_F(SearchTest, QuiescenceTranspositionTableDoesNotCutOffPvNode) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    const int      stand_pat    = evaluate(board);
    const int      stored_score = stand_pat + 500;

    loadThreadBoard(board);
    tt.store(threadKey(), NULL_MOVE, stored_score, 0, TT_Flag::Exact, 0);

    EXPECT_EQ(runLoadedPvQuiescence(-INF_VALUE, INF_VALUE), stand_pat)
        << "PV qsearch should not return a stored TT score without searching";
}

TEST_F(SearchTest, QuiescenceTranspositionTableStoresStandPatLowerbound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    const int      stand_pat = evaluate(board);
    const int      alpha     = stand_pat - 100;
    const int      beta      = stand_pat - 50;

    loadThreadBoard(board);
    EXPECT_EQ(runLoadedQuiescence(alpha, beta), stand_pat);

    auto entry = tt.probe(threadKey());
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->flag, TT_Flag::Lowerbound);
    EXPECT_EQ(entry->depth, 0);
    EXPECT_EQ(entry->move, NULL_MOVE);
    EXPECT_EQ(entry->get_score(0), stand_pat);
}

TEST_F(SearchTest, QuiescenceTranspositionTableStoresFailLowUpperbound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    const int      stand_pat = evaluate(board);
    const int      alpha     = stand_pat + 100;
    const int      beta      = stand_pat + 200;

    loadThreadBoard(board);
    EXPECT_EQ(runLoadedQuiescence(alpha, beta), stand_pat);

    auto entry = tt.probe(threadKey());
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->flag, TT_Flag::Upperbound);
    EXPECT_EQ(entry->depth, 0);
    EXPECT_EQ(entry->move, NULL_MOVE);
    EXPECT_EQ(entry->get_score(0), stand_pat);
}

TEST_F(SearchTest, QuiescenceTranspositionTableStoresExactWhenScoreRaisesAlpha) {
    constexpr auto winning_capture = "k7/8/8/8/8/8/4r3/K2Q4 w - - 0 1";
    TestBoard      capture_board{winning_capture};
    const int      stand_pat = evaluate(capture_board);
    const int      alpha     = stand_pat - 50;
    const int      beta      = INF_VALUE;

    const int expected = -testQuiescenceAfterMove(winning_capture, "d1e2", -beta, -alpha);
    ASSERT_GT(expected, alpha) << winning_capture;
    ASSERT_LT(expected, beta) << winning_capture;

    TestBoard board{winning_capture};
    loadThreadBoard(board);

    EXPECT_EQ(runLoadedQuiescence(alpha, beta), expected);

    auto entry = tt.probe(threadKey());
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->flag, TT_Flag::Exact);
    EXPECT_EQ(entry->depth, 0);
    EXPECT_EQ(entry->move, NULL_MOVE);
    EXPECT_EQ(entry->get_score(0), expected);
}

TEST_F(SearchTest, QuiescenceTranspositionTableStoresMateWithPlyAdjustment) {
    constexpr auto checkmate = "k7/1Q6/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{checkmate};
    loadThreadBoard(board);
    ASSERT_TRUE(threadInCheck()) << checkmate;

    constexpr int search_ply = 4;
    constexpr int mate_score = -MATE_VALUE + search_ply;
    setThreadPly(search_ply);

    EXPECT_EQ(runLoadedQuiescence(-INF_VALUE, INF_VALUE), mate_score);

    auto entry = tt.probe(threadKey());
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->flag, TT_Flag::Exact);
    EXPECT_EQ(entry->depth, 0);
    EXPECT_EQ(entry->move, NULL_MOVE);
    EXPECT_EQ(entry->get_score(search_ply), mate_score);
}

TEST_F(SearchTest, QuiescenceTranspositionTableStoresDrawForStalemate) {
    constexpr auto stalemate = "k7/8/KQ6/8/8/8/8/8 b - - 0 1";
    TestBoard      board{stalemate};
    const int      stand_pat = evaluate(board);
    const int      alpha     = stand_pat - 100;
    const int      beta      = stand_pat - 1;

    loadThreadBoard(board);
    ASSERT_TRUE(board.is_stalemate()) << stalemate;
    EXPECT_EQ(runLoadedQuiescence(alpha, beta), DRAW_VALUE);

    auto entry = tt.probe(threadKey());
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->flag, TT_Flag::Exact);
    EXPECT_EQ(entry->depth, 0);
    EXPECT_EQ(entry->move, NULL_MOVE);
    EXPECT_EQ(entry->get_score(0), DRAW_VALUE);
}

TEST_F(SearchTest, QuiescenceTranspositionTableDoesNotOverrideDrawDetection) {
    constexpr auto drawn_by_fifty_move = "k7/8/2K5/8/8/8/8/8 b - - 100 1";
    TestBoard      board{drawn_by_fifty_move};
    loadThreadBoard(board);
    ASSERT_TRUE(board.is_draw()) << drawn_by_fifty_move;

    tt.store(threadKey(), NULL_MOVE, 1234, 0, TT_Flag::Exact, 0);

    EXPECT_EQ(runLoadedQuiescence(-INF_VALUE, INF_VALUE), DRAW_VALUE);
}

#if LATRUNCULI_SEARCH_STATS
TEST_F(SearchTest, QuiescenceTranspositionTableStatsCountersIncrement) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadThreadBoard(board);
    resetThreadStats();

    constexpr int stored_score = 321;
    tt.store(threadKey(), NULL_MOVE, stored_score, 0, TT_Flag::Exact, 0);

    EXPECT_EQ(runLoadedQuiescence(-INF_VALUE, INF_VALUE), stored_score);
    EXPECT_EQ(qTtProbes(0), 1);
    EXPECT_EQ(qTtHits(0), 1);
    EXPECT_EQ(qTtCutoffs(0), 1);
}
#endif

TEST_F(SearchTest, AlphaBetaFailLowReturnsBestMoveScoreInsteadOfAlpha) {
    constexpr auto one_legal_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  search_depth      = 1;
    constexpr int  alpha             = -100;
    constexpr int  beta              = 0;

    const int expected =
        -testAlphaBetaAfterMove(one_legal_evasion, "a8b8", -beta, -alpha, search_depth);
    ASSERT_LT(expected, alpha) << one_legal_evasion;

    TestBoard board{one_legal_evasion};
    loadThreadBoard(board);
    ASSERT_TRUE(threadInCheck()) << one_legal_evasion;

    const int actual = runNonPvAlphaBeta(alpha, beta, search_depth, true);
    EXPECT_EQ(actual, expected) << one_legal_evasion;
    EXPECT_NE(actual, alpha) << "fail-low should return the discovered child score, not alpha";
}

TEST_F(SearchTest, AlphaBetaFailHighReturnsRawScoreInsteadOfBeta) {
    constexpr auto one_legal_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  search_depth      = 1;

    TestBoard exact_board{one_legal_evasion};
    loadThreadBoard(exact_board);
    const int exact = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth, true);

    const int beta  = exact - 50;
    const int alpha = beta - 100;
    const int expected =
        -testAlphaBetaAfterMove(one_legal_evasion, "a8b8", -beta, -alpha, search_depth);
    ASSERT_GT(expected, beta) << one_legal_evasion;

    TestBoard board{one_legal_evasion};
    loadThreadBoard(board);
    ASSERT_TRUE(threadInCheck()) << one_legal_evasion;

    const int actual = runNonPvAlphaBeta(alpha, beta, search_depth, true);
    EXPECT_EQ(actual, expected) << one_legal_evasion;
    EXPECT_NE(actual, beta) << "fail-high should return the raw child score, not beta";
}

TEST_F(SearchTest, QuiescenceMateDistanceLowerClampReturnsBound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadThreadBoard(board);

    constexpr int search_ply  = 4;
    constexpr int lower_bound = -MATE_VALUE + search_ply;
    setThreadPly(search_ply);

    EXPECT_EQ(runLoadedQuiescence(-INF_VALUE, lower_bound - 1), lower_bound);
}

TEST_F(SearchTest, QuiescenceMateDistanceUpperClampReturnsBound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadThreadBoard(board);

    constexpr int search_ply  = 4;
    constexpr int upper_bound = MATE_VALUE - search_ply - 1;
    setThreadPly(search_ply);

    EXPECT_EQ(runLoadedQuiescence(upper_bound + 1, INF_VALUE), upper_bound);
}

TEST_F(SearchTest, QuiescenceMateDistanceUpperClampAtRootReturnsMateInOneBound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadThreadBoard(board);

    constexpr int root_ply    = 0;
    constexpr int upper_bound = MATE_VALUE - root_ply - 1;
    setThreadPly(root_ply);

    EXPECT_EQ(runLoadedQuiescence(upper_bound + 1, INF_VALUE), upper_bound);
}

TEST_F(SearchTest, AlphaBetaMateDistanceLowerClampReturnsBound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadThreadBoard(board);

    constexpr int search_ply  = 4;
    constexpr int lower_bound = -MATE_VALUE + search_ply;
    setThreadPly(search_ply);

    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, lower_bound - 1, 1, true), lower_bound);
}

TEST_F(SearchTest, AlphaBetaMateDistanceUpperClampReturnsBound) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    TestBoard      board{quiet_control};
    loadThreadBoard(board);

    constexpr int search_ply  = 4;
    constexpr int upper_bound = MATE_VALUE - search_ply - 1;
    setThreadPly(search_ply);

    EXPECT_EQ(runNonPvAlphaBeta(upper_bound + 1, INF_VALUE, 1, true), upper_bound);
}

TEST_F(SearchTest, QuiescenceInCheckAtMaxDepthReturnsStaticEval) {
    constexpr auto in_check_with_one_quiet_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    TestBoard      board{in_check_with_one_quiet_evasion};
    loadThreadBoard(board);

    ASSERT_TRUE(threadInCheck()) << in_check_with_one_quiet_evasion;
    const int static_eval = evaluate(board);

    setThreadPly(MAX_SEARCH_PLY);

    EXPECT_EQ(runLoadedQuiescence(-INF_VALUE, INF_VALUE), static_eval);
}

TEST_F(SearchTest, QuiescenceStalemateStandPatFailHighReturnsDraw) {
    constexpr auto stalemate = "k7/8/KQ6/8/8/8/8/8 b - - 0 1";
    TestBoard      board{stalemate};

    ASSERT_FALSE(board.is_check()) << stalemate;
    ASSERT_TRUE(board.is_stalemate()) << stalemate;

    const int stand_pat = evaluate(board);
    ASSERT_NE(stand_pat, DRAW_VALUE) << stalemate;

    const int beta  = stand_pat - 1;
    const int alpha = beta - 100;

    EXPECT_EQ(testQuiescence(stalemate, alpha, beta), DRAW_VALUE);
}

TEST_F(SearchTest, RootSearchIgnoresTranspositionTableBoundsExceptForOrdering) {
    constexpr auto one_legal_root_move = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  search_depth        = 1;

    TestBoard baseline_board{one_legal_root_move};
    loadThreadBoard(baseline_board);
    const int baseline = runRootSearchWiden(search_depth, evaluate(baseline_board));

    const int alpha = baseline - 50;
    const int beta  = baseline + 50;
    ASSERT_LT(alpha, baseline);
    ASSERT_GT(beta, baseline);

    auto search_with_root_tt = [&](TT_Flag flag, int score) {
        TestBoard board{one_legal_root_move};
        loadThreadBoard(board);
        Move root_move = findThreadMove("a8b8");
        EXPECT_FALSE(root_move.is_null());

        tt.store(threadKey(), root_move, score, search_depth + 2, flag, 0);
        return runRootSearchWiden(search_depth, baseline);
    };

    EXPECT_EQ(search_with_root_tt(TT_Flag::Lowerbound, beta + 500), baseline);
    EXPECT_EQ(search_with_root_tt(TT_Flag::Upperbound, alpha - 500), baseline);
    EXPECT_EQ(search_with_root_tt(TT_Flag::Exact, baseline + 500), baseline);
}

TEST_F(SearchTest, RootSearchWidenResearchesFailedAspirationWindows) {
    constexpr auto one_legal_root_move = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    constexpr int  search_depth        = 1;

    TestBoard baseline_board{one_legal_root_move};
    loadThreadBoard(baseline_board);
    const int baseline = runRootSearchWiden(search_depth, evaluate(baseline_board));

    TestBoard fail_low_board{one_legal_root_move};
    loadThreadBoard(fail_low_board);
    EXPECT_EQ(runRootSearchWiden(search_depth, baseline + 1000), baseline);

    TestBoard fail_high_board{one_legal_root_move};
    loadThreadBoard(fail_high_board);
    EXPECT_EQ(runRootSearchWiden(search_depth, baseline - 1000), baseline);
}

TEST_F(SearchTest, HaltedSearchReturnsAbortSentinelValues) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  alpha         = -123;
    constexpr int  beta          = 456;

    TestBoard alpha_beta_board{quiet_control};
    loadThreadBoard(alpha_beta_board);
    thread->halt();
    EXPECT_EQ(runNonPvAlphaBeta(alpha, beta, 2, true), alpha);

    TestBoard qsearch_board{quiet_control};
    loadThreadBoard(qsearch_board);
    thread->halt();
    EXPECT_EQ(runLoadedQuiescence(alpha, beta), alpha);
}

TEST_F(SearchTest, TranspositionTableExactWithNonPseudoLegalMoveStillCutsOff) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 1;

    TestBoard baseline_board{quiet_control};
    loadThreadBoard(baseline_board);
    const int baseline = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth, false);

    TestBoard board{quiet_control};
    loadThreadBoard(board);

    Move invalid_tt_move{H1, H2};
    ASSERT_FALSE(board.is_legal_move(invalid_tt_move));
    ASSERT_FALSE(board.is_pseudo_legal(invalid_tt_move));

    const int bogus_score = baseline + 500;
    ASSERT_NE(bogus_score, baseline);

    tt.store(threadKey(), invalid_tt_move, bogus_score, search_depth + 2, TT_Flag::Exact, 0);

    EXPECT_EQ(runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth, false), bogus_score);
}

TEST_F(SearchTest, PrincipalVariationSkipsNonPseudoLegalTranspositionTableMove) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";

    TestBoard board{quiet_control};
    loadThreadBoard(board);

    Move invalid_tt_move{H1, H2};
    ASSERT_FALSE(board.is_legal_move(invalid_tt_move));
    ASSERT_FALSE(board.is_pseudo_legal(invalid_tt_move));

    tt.store(threadKey(), invalid_tt_move, 0, 1, TT_Flag::Exact, 0);

    EXPECT_EQ(currentPv(1), "");
}

TEST_F(SearchTest, PrincipalVariationPrintsPseudoLegalTranspositionTableMove) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";

    TestBoard board{quiet_control};
    loadThreadBoard(board);

    Move pv_move = findThreadMove("a8b8");
    ASSERT_FALSE(pv_move.is_null());
    ASSERT_TRUE(threadMoveIsPseudoLegal(pv_move));
    ASSERT_TRUE(threadMoveIsLegal(pv_move));

    tt.store(threadKey(), pv_move, 0, 1, TT_Flag::Exact, 0);

    EXPECT_EQ(currentPv(1), "a8b8 ");
}

TEST_F(SearchTest, NonPvTranspositionTableBoundsAreCutoffOnly) {
    constexpr auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    constexpr int  search_depth  = 2;

    TestBoard baseline_board{quiet_control};
    loadThreadBoard(baseline_board);
    const int baseline = runNonPvAlphaBeta(-INF_VALUE, INF_VALUE, search_depth, false);

    const int alpha = baseline - 100;
    const int beta  = baseline + 100;
    ASSERT_LT(alpha, baseline);
    ASSERT_GT(beta, baseline);

    auto search_with_tt_bound = [&](TT_Flag flag, int score) {
        TestBoard board{quiet_control};
        loadThreadBoard(board);
        Move tt_move = findThreadMove("a8b8");
        EXPECT_FALSE(tt_move.is_null());

        tt.store(threadKey(), tt_move, score, search_depth + 2, flag, 0);
        return runNonPvAlphaBeta(alpha, beta, search_depth, false);
    };

    EXPECT_EQ(search_with_tt_bound(TT_Flag::Lowerbound, baseline + 50), baseline);
    EXPECT_EQ(search_with_tt_bound(TT_Flag::Upperbound, baseline - 50), baseline);
    EXPECT_EQ(search_with_tt_bound(TT_Flag::Lowerbound, beta + 25), beta + 25);
    EXPECT_EQ(search_with_tt_bound(TT_Flag::Upperbound, alpha - 25), alpha - 25);
}

TEST_F(SearchTest, NullMoveDisablesOnlyImmediateChildAndReenablesLaterDescendants) {
    constexpr auto immediate_null_child =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 1";
    TestBoard board{immediate_null_child};
    loadThreadBoard(board);

    ASSERT_FALSE(threadInCheck());
    ASSERT_GT(threadNonPawnMaterial(), BISHOP_MG);

    Move real_move = findThreadMove("e7e5");
    ASSERT_FALSE(real_move.is_null());
    ASSERT_FALSE(threadDescendantInCheck(real_move));
    ASSERT_GT(threadDescendantNonPawnMaterial(real_move), BISHOP_MG);

    const auto immediate_null_key  = threadNullChildKey();
    const auto descendant_null_key = threadDescendantNullKey(real_move);

    ASSERT_FALSE(tt.probe(immediate_null_key).has_value());
    ASSERT_FALSE(tt.probe(descendant_null_key).has_value());

    tt.store(threadKey(), real_move, 0, 0, TT_Flag::Lowerbound, 0);
    (void)runNonPvAlphaBeta(-50, 50, 5, false);

    EXPECT_FALSE(tt.probe(immediate_null_key).has_value())
        << "the immediate null child must still forbid a second null move";
    EXPECT_TRUE(tt.probe(descendant_null_key).has_value())
        << "after a real move from the null child, descendants should regain null-move eligibility";
}

TEST_F(SearchTest, basicTactics) {
    auto searchpos1 = "k7/4r3/8/8/8/3Q4/4p3/K7 w - -";
    auto searchpos2 = "3r4/pbb1qBk1/2p4p/1p2N1p1/3r4/P3Q2P/1P3PP1/2RR2K1 w - -";

    std::vector<std::tuple<std::string, int, std::string>> test_cases = {
        {searchpos1, eval::piece(ROOK).mg, "d3d8"},
        {searchpos2, eval::piece(PAWN).mg, "d1d4"},
    };

    for (auto& [fen, expectedScore, expectedMove] : test_cases) {
        testSearchGT(fen, expectedScore, expectedMove);
    }
}
