
#include <algorithm>

#include <gtest/gtest.h>

#include "board.hpp"
#include "evaluator.hpp"

#include "search_options.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "tt.hpp"
#include "uci.hpp"
#include "test_util.hpp"

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
        Board board{fen};
        options.board = &board;
        thread->set_options(options);
        thread->reset();
        return thread->quiescence(alpha, beta);
    }

    int testQuiescenceAfterMove(const std::string& fen, const std::string& move_str, int alpha = -INF_VALUE, int beta = INF_VALUE) {
        Board board{fen};
        options.board = &board;
        thread->set_options(options);
        thread->reset();

        auto movelist = generate<ALL_MOVES>(thread->board);
        auto move_it = std::find_if(movelist.begin(), movelist.end(), [&](const Move& move) {
            return move.str() == move_str && thread->board.is_legal_move(move);
        });
        EXPECT_NE(move_it, movelist.end()) << fen;
        if (move_it == movelist.end())
            return 0;

        thread->board.make(*move_it);
        int score = thread->quiescence(alpha, beta);
        thread->board.unmake();
        return score;
    }

    std::pair<int, uint64_t> testQuiescenceWithNodes(const std::string& fen, int alpha = -INF_VALUE, int beta = INF_VALUE) {
        Board board{fen};
        options.board = &board;
        thread->set_options(options);
        thread->reset();
        return {thread->quiescence(alpha, beta), thread->nodes};
    }

    Move firstLegalPickedMove(Board& board, Move pv_move = NULL_MOVE, Move tt_move = NULL_MOVE,
                              bool root = false, int thread_id = 0) {
        MoveList          movelist = generate<ALL_MOVES>(board);
        StagedMovePicker  picker{{board, sort_killers, sort_history, sort_ply, pv_move, tt_move, root, thread_id}, std::move(movelist)};

        while (Move* move = picker.next()) {
            if (board.is_legal_move(*move))
                return *move;
        }
        return NULL_MOVE;
    }

    std::vector<Move> pickedLegalQuiescenceMoves(Board& board) {
        const bool in_check = board.is_check();
        MoveList   movelist = in_check ? generate<EVASIONS>(board) : generate<CAPTURES>(board);
        QuiescenceMovePicker picker{{board, sort_killers, sort_history, sort_ply}, std::move(movelist), in_check};

        std::vector<Move> moves;
        while (Move* move = picker.next()) {
            if (board.is_legal_move(*move))
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
            return move.str() == move_str && thread->board.is_legal_move(move);
        });
        EXPECT_NE(move_it, movelist.end()) << thread->board.toFEN();
        return move_it == movelist.end() ? NULL_MOVE : *move_it;
    }

    bool threadInCheck() const { return thread->board.is_check(); }

    int threadNonPawnMaterial() const { return thread->board.nonPawnMaterial(thread->board.side_to_move()); }

    uint64_t threadKey() const { return thread->board.key(); }

    uint64_t threadNullChildKey() const {
        Board board_copy{thread->board.toFEN()};
        board_copy.make_null();
        return board_copy.key();
    }

    bool threadDescendantInCheck(Move move) const {
        Board board_copy{thread->board.toFEN()};
        board_copy.make(move);
        return board_copy.is_check();
    }

    int threadDescendantNonPawnMaterial(Move move) const {
        Board board_copy{thread->board.toFEN()};
        board_copy.make(move);
        return board_copy.nonPawnMaterial(board_copy.side_to_move());
    }

    uint64_t threadDescendantNullKey(Move move) const {
        Board board_copy{thread->board.toFEN()};
        board_copy.make(move);
        board_copy.make_null();
        return board_copy.key();
    }

    int runNonPvAlphaBeta(int alpha, int beta, int search_depth, bool can_null) {
        return thread->alphabeta<NON_PV>(alpha, beta, search_depth, can_null);
    }

    void testSearch(const std::string fen, int score, std::string move) {
        Board board{fen};
        options.board = &board;
        thread->start(options);
        thread->wait();

        EXPECT_EQ(thread->root_value, score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(thread->root_move.str(), move) << fen;
        }
    }

    void testSearchGT(const std::string fen, int score, std::string move) {
        Board board{fen};
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
    auto in_check_with_one_quiet_evasion = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
    Board board{in_check_with_one_quiet_evasion};

    ASSERT_TRUE(board.is_check()) << in_check_with_one_quiet_evasion;

    int actual = testQuiescence(in_check_with_one_quiet_evasion);
    int expected = -testQuiescenceAfterMove(in_check_with_one_quiet_evasion, "a8b8");

    EXPECT_EQ(actual, expected) << in_check_with_one_quiet_evasion;
    EXPECT_NE(actual, evaluate(board)) << "in-check qsearch must not stand pat";
}

TEST_F(SearchTest, QuiescenceOutOfCheckUsesStandPatInsteadOfSearchingQuiets) {
    auto quiet_control = "k7/8/2K5/8/8/8/8/8 b - - 0 1";
    Board board{quiet_control};

    ASSERT_FALSE(board.is_check()) << quiet_control;

    EXPECT_EQ(testQuiescence(quiet_control), evaluate(board)) << quiet_control;
}

TEST_F(SearchTest, FirstLegalPickedMoveSkipsIllegalHashMove) {
    Board board{POS3};
    Move  illegal_hash_move = Move(B5, B6);

    ASSERT_FALSE(board.is_legal_move(illegal_hash_move));

    EXPECT_EQ(firstLegalPickedMove(board, NULL_MOVE, illegal_hash_move), Move(B4, F4));
}

TEST_F(SearchTest, QuiescenceOutOfCheckIncludesPromotions) {
    constexpr auto promotion_fen = "7k/P7/8/8/8/8/8/4K3 w - - 0 1";
    Board          board{promotion_fen};

    ASSERT_FALSE(board.is_check()) << promotion_fen;

    const auto legal_moves = pickedLegalQuiescenceMoves(board);
    ASSERT_EQ(legal_moves.size(), 4U) << promotion_fen;
    EXPECT_EQ(legal_moves[0].str(), "a7a8q");
    EXPECT_EQ(legal_moves[1].str(), "a7a8r");
    EXPECT_EQ(legal_moves[2].str(), "a7a8b");
    EXPECT_EQ(legal_moves[3].str(), "a7a8n");

    const int actual = testQuiescence(promotion_fen);
    const int expected = std::max({-testQuiescenceAfterMove(promotion_fen, "a7a8q"),
                                   -testQuiescenceAfterMove(promotion_fen, "a7a8r"),
                                   -testQuiescenceAfterMove(promotion_fen, "a7a8b"),
                                   -testQuiescenceAfterMove(promotion_fen, "a7a8n")});

    EXPECT_EQ(actual, expected) << promotion_fen;
}

TEST_F(SearchTest, QuiescenceOutOfCheckSkipsWeakCaptures) {
    constexpr auto weak_capture_fen = "2b3k1/3p4/8/8/8/8/8/3Q2K1 w - - 0 1";
    Board          board{weak_capture_fen};

    loadThreadBoard(board);
    ASSERT_FALSE(threadInCheck()) << weak_capture_fen;

    const auto legal_captures = pickedLegalQuiescenceMoves(board);
    ASSERT_EQ(legal_captures.size(), 1U) << weak_capture_fen;
    EXPECT_EQ(legal_captures[0].str(), "d1d7");
    EXPECT_EQ(legal_captures[0].priority, PRIORITY_WEAK);

    const int                stand_pat = evaluate(board);
    const auto [score, nodes] = testQuiescenceWithNodes(weak_capture_fen);

    EXPECT_EQ(score, stand_pat);
    EXPECT_EQ(nodes, 1U) << "weak captures should be pruned before recursive qsearch";
}

TEST_F(SearchTest, NullMoveDisablesOnlyImmediateChildAndReenablesLaterDescendants) {
    constexpr auto immediate_null_child = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 1";
    Board          board{immediate_null_child};
    loadThreadBoard(board);

    ASSERT_FALSE(threadInCheck());
    ASSERT_GT(threadNonPawnMaterial(), BISHOP_MG);

    Move real_move = findThreadMove("e7e5");
    ASSERT_FALSE(real_move.is_null());
    ASSERT_FALSE(threadDescendantInCheck(real_move));
    ASSERT_GT(threadDescendantNonPawnMaterial(real_move), BISHOP_MG);

    const auto immediate_null_key = threadNullChildKey();
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
