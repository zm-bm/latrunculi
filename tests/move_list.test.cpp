#include "move_list.hpp"

#include <gtest/gtest.h>

#include "board.hpp"
#include "movegen.hpp"
#include "test_util.hpp"

class MoveListTest : public ::testing::Test {
protected:
    int          ply = 5;
    Board        board{POS3};
    KillerMoves  killers;
    HistoryTable history;
};

TEST_F(MoveListTest, GenerateSortMoves) {
    MoveList movelist = generate<ALL_MOVES>(board);
    movelist.sort({board, killers, history, ply});

    EXPECT_FALSE(movelist.empty());
}

TEST_F(MoveListTest, SortMovesWithHistoryAndKiller) {
    Move killer_move = Move(A5, A4);
    killers.update(killer_move, ply);

    Move hist_move = Move(A5, A6);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    MoveList movelist = generate<ALL_MOVES>(board);
    movelist.sort({board, killers, history, ply});

    EXPECT_GT(movelist.size(), 3);
    EXPECT_EQ(movelist[0], Move(B4, F4));
    EXPECT_EQ(movelist[1], killer_move);
    EXPECT_EQ(movelist[2], hist_move);
}

TEST_F(MoveListTest, SortMovesWithPVAndHash) {
    Move pvMove   = Move(B4, C4);
    Move hashMove = Move(E2, E3);

    MoveList movelist = generate<ALL_MOVES>(board);
    movelist.sort({board, killers, history, ply, pvMove, hashMove});

    EXPECT_GT(movelist.size(), 1);
    EXPECT_EQ(movelist[0], hashMove);
    EXPECT_EQ(movelist[1], pvMove);
}
