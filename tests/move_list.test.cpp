#include "move_list.hpp"

#include <vector>

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

namespace {
std::vector<Move> ordered_moves(MoveList& movelist) {
    std::vector<Move> moves;
    for (const auto& move : movelist)
        moves.push_back(move);
    return moves;
}
} // namespace

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
    EXPECT_EQ(movelist[0], pvMove);
    EXPECT_EQ(movelist[1], hashMove);
}

TEST_F(MoveListTest, HelperRootSortRotatesEqualPriorityMoves) {
    Board quiet_board{STARTFEN};

    MoveList main_root = generate<ALL_MOVES>(quiet_board);
    main_root.sort({quiet_board, killers, history, ply, NULL_MOVE, NULL_MOVE, true, 0});

    MoveList helper_root = generate<ALL_MOVES>(quiet_board);
    helper_root.sort({quiet_board, killers, history, ply, NULL_MOVE, NULL_MOVE, true, 1});

    auto main_order   = ordered_moves(main_root);
    auto helper_order = ordered_moves(helper_root);

    ASSERT_GT(main_order.size(), 1);
    EXPECT_NE(helper_order, main_order);
    EXPECT_EQ(helper_order.front(), main_order[1]);
}

TEST_F(MoveListTest, MainThreadAndNonRootSortKeepOriginalOrder) {
    Board quiet_board{STARTFEN};

    MoveList default_order = generate<ALL_MOVES>(quiet_board);
    default_order.sort({quiet_board, killers, history, ply});

    MoveList main_root = generate<ALL_MOVES>(quiet_board);
    main_root.sort({quiet_board, killers, history, ply, NULL_MOVE, NULL_MOVE, true, 0});

    MoveList helper_non_root = generate<ALL_MOVES>(quiet_board);
    helper_non_root.sort({quiet_board, killers, history, ply, NULL_MOVE, NULL_MOVE, false, 1});

    EXPECT_EQ(ordered_moves(main_root), ordered_moves(default_order));
    EXPECT_EQ(ordered_moves(helper_non_root), ordered_moves(default_order));
}

TEST_F(MoveListTest, HelperRootSortKeepsPVAndHashFirst) {
    Move pvMove   = Move(B4, C4);
    Move hashMove = Move(E2, E3);

    MoveList helper_root = generate<ALL_MOVES>(board);
    helper_root.sort({board, killers, history, ply, pvMove, hashMove, true, 1});

    ASSERT_GT(helper_root.size(), 1);
    EXPECT_EQ(helper_root[0], pvMove);
    EXPECT_EQ(helper_root[1], hashMove);
}
