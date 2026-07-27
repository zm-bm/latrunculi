#include <gtest/gtest.h>

#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"
#include "support/board_snapshot.hpp"

TEST(BoardDrawTest, AppliesFiftyMoveRule) {
    EXPECT_TRUE(board_test::Harness("k7/8/8/8/8/8/8/K7 w - - 100 50").is_draw());
    EXPECT_FALSE(board_test::Harness("k7/8/8/8/8/8/8/K7 w - - 99 50").is_draw());
}

TEST(BoardDrawTest, DetectsThreefoldRepetition) {
    board_test::Harness board(board_test::fen::repetition_cycle);

    board.make(Move(E6, F5));
    board.make(Move(H7, G8));
    board.make(Move(F5, E6));
    board.make(Move(G8, H7));
    board.make(Move(E6, F5));
    EXPECT_FALSE(board.is_draw());
    board.make(Move(H7, G8));
    EXPECT_FALSE(board.is_draw());
    board.make(Move(F5, E6));
    EXPECT_FALSE(board.is_draw());
    board.make(Move(G8, H7));
    EXPECT_FALSE(board.is_draw());
    board.make(Move(E6, F5));
    EXPECT_TRUE(board.is_draw());
}

TEST(BoardDrawTest, TwofoldRootHistoryIsNotDraw) {
    board_test::Harness board(board_test::fen::corner_kings);

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
    EXPECT_FALSE(board.is_draw());
    EXPECT_FALSE(board.is_draw(4));
    EXPECT_TRUE(board.is_draw(5));

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
    EXPECT_TRUE(board.is_draw());
}

TEST(BoardDrawTest, NullMoveUnmakeRestoresRepetitionHistory) {
    board_test::Harness board(board_test::fen::corner_kings);

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
    EXPECT_FALSE(board.is_draw());

    board.make_null();
    board.unmake_null();
    EXPECT_FALSE(board.is_draw());

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));

    EXPECT_TRUE(board.is_draw());
}

TEST(BoardDrawTest, UnmakeIrreversibleMovePreservesPriorRepetitionHistory) {
    board_test::Harness board("7k/8/8/8/8/8/P7/K7 w - - 0 1");

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
    ASSERT_FALSE(board.is_draw());
    ASSERT_TRUE(board.is_draw(5));
    const auto before = board_test::snapshot_board(board);

    board.make(Move(A2, A3));
    board.make(Move(H8, G8));
    board.make(Move(A1, B1));
    EXPECT_FALSE(board.is_draw());

    board.unmake();
    board.unmake();
    board.unmake();

    board_test::expect_same_board_snapshot(board, before);
    EXPECT_FALSE(board.is_draw());
    EXPECT_TRUE(board.is_draw(5));
}
