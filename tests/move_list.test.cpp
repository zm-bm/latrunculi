#include "move_list.hpp"

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "board.hpp"
#include "movegen.hpp"
#include "test_util.hpp"

class MoveListTest : public ::testing::Test {
protected:
    int          ply = 5;
    TestBoard    board{POS3};
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

std::vector<Move> picked_moves(Board&        board,
                               KillerMoves&  killers,
                               HistoryTable& history,
                               int           ply,
                               Move          pv_move   = NULL_MOVE,
                               Move          tt_move   = NULL_MOVE,
                               bool          root      = false,
                               int           thread_id = 0) {
    std::vector<Move> moves;
    MoveList          movelist = generate<ALL_MOVES>(board);
    StagedMovePicker  picker{{board, killers, history, ply, pv_move, tt_move, root, thread_id},
                            std::move(movelist)};

    while (Move* move = picker.next())
        moves.push_back(*move);

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

TEST_F(MoveListTest, SaturatedHistoryQuietStaysBelowKillerPriority) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    for (int i = 0; i < 8; ++i)
        history.update(board.side_to_move(), hist_move.from(), hist_move.to(), MAX_SEARCH_DEPTH);

    MoveList movelist = generate<ALL_MOVES>(board);
    movelist.sort({board, killers, history, ply});

    const auto moves     = ordered_moves(movelist);
    const auto killer_it = std::find(moves.begin(), moves.end(), killer_move);
    const auto hist_it   = std::find(moves.begin(), moves.end(), hist_move);

    ASSERT_NE(killer_it, moves.end());
    ASSERT_NE(hist_it, moves.end());
    EXPECT_LT(killer_it, hist_it);
    EXPECT_LE(history.get(board.side_to_move(), hist_move.from(), hist_move.to()),
              PRIORITY_HISTORY);
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
    TestBoard quiet_board{STARTFEN};

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
    TestBoard quiet_board{STARTFEN};

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

TEST_F(MoveListTest, HelperRootSortKeepsHashMoveFirstWithoutPV) {
    TestBoard quiet_board{STARTFEN};
    Move      hash_move = Move(E2, E4);

    MoveList helper_root = generate<ALL_MOVES>(quiet_board);
    helper_root.sort({quiet_board, killers, history, ply, NULL_MOVE, hash_move, true, 1});

    ASSERT_FALSE(helper_root.empty());
    EXPECT_EQ(helper_root[0], hash_move);
}

TEST_F(MoveListTest, StagedPickerKeepsPVHashKillerAndHistoryOrder) {
    Move pv_move     = Move(B4, C4);
    Move hash_move   = Move(E2, E3);
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves = picked_moves(board, killers, history, ply, pv_move, hash_move);

    ASSERT_GT(moves.size(), 4U);
    EXPECT_EQ(moves[0], pv_move);
    EXPECT_EQ(moves[1], hash_move);
    EXPECT_EQ(moves[2], Move(B4, F4));
    EXPECT_EQ(moves[3], killer_move);
    EXPECT_EQ(moves[4], hist_move);
}

TEST_F(MoveListTest, HelperRootPickerRotatesEqualPriorityMovesButKeepsPVHashFirst) {
    TestBoard quiet_board{STARTFEN};
    Move      hash_move = Move(E2, E4);

    const auto main_order =
        picked_moves(quiet_board, killers, history, ply, NULL_MOVE, hash_move, true, 0);
    const auto helper_order =
        picked_moves(quiet_board, killers, history, ply, NULL_MOVE, hash_move, true, 1);

    ASSERT_GT(main_order.size(), 2U);
    ASSERT_EQ(main_order.front(), hash_move);
    ASSERT_EQ(helper_order.front(), hash_move);
    EXPECT_NE(helper_order, main_order);
    EXPECT_EQ(helper_order[1], main_order[2]);
}
