#include "move_picker.hpp"

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "movegen.hpp"
#include "test_util.hpp"

class MovePickerTest : public ::testing::Test {
protected:
    int          ply = 5;
    TestBoard    board{POS3};
    KillerMoves  killers;
    HistoryTable history;
};

namespace {
std::vector<Move> picked_moves(Board&         board,
                               KillerMoves&   killers,
                               HistoryTable&  history,
                               int            ply,
                               MovePickerMode mode,
                               Move           pv_move   = NULL_MOVE,
                               Move           tt_move   = NULL_MOVE,
                               bool           root      = false,
                               int            thread_id = 0,
                               GeneratorMode  generator = ALL_MOVES) {
    std::vector<Move> moves;
    MoveList          movelist = generator == EVASIONS   ? generate<EVASIONS>(board)
                                 : generator == CAPTURES ? generate<CAPTURES>(board)
                                                         : generate<ALL_MOVES>(board);
    MovePicker        picker{
               {board, killers, history, ply, pv_move, tt_move, root, thread_id}, movelist, mode};

    while (Move* move = picker.next())
        moves.push_back(*move);

    return moves;
}
} // namespace

TEST_F(MovePickerTest, MainSearchPicksGeneratedMoves) {
    const auto moves = picked_moves(board, killers, history, ply, MovePickerMode::MainSearch);

    EXPECT_FALSE(moves.empty());
}

TEST_F(MovePickerTest, MainSearchKeepsHistoryBelowKillerPriority) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    for (int i = 0; i < 8; ++i)
        history.update(board.side_to_move(), hist_move.from(), hist_move.to(), MAX_SEARCH_DEPTH);

    const auto moves     = picked_moves(board, killers, history, ply, MovePickerMode::MainSearch);
    const auto killer_it = std::find(moves.begin(), moves.end(), killer_move);
    const auto hist_it   = std::find(moves.begin(), moves.end(), hist_move);

    ASSERT_NE(killer_it, moves.end());
    ASSERT_NE(hist_it, moves.end());
    EXPECT_LT(killer_it, hist_it);
    EXPECT_LE(history.get(board.side_to_move(), hist_move.from(), hist_move.to()),
              PRIORITY_HISTORY);
}

TEST_F(MovePickerTest, MainSearchKeepsCaptureKillerAndHistoryOrder) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves = picked_moves(board, killers, history, ply, MovePickerMode::MainSearch);

    ASSERT_GT(moves.size(), 3U);
    EXPECT_EQ(moves[0], Move(B4, F4));
    EXPECT_EQ(moves[1], killer_move);
    EXPECT_EQ(moves[2], hist_move);
}

TEST_F(MovePickerTest, MainSearchKeepsPVAndHashFirst) {
    Move pv_move = Move(B4, C4);
    Move tt_move = Move(E2, E3);

    const auto moves =
        picked_moves(board, killers, history, ply, MovePickerMode::MainSearch, pv_move, tt_move);

    ASSERT_GT(moves.size(), 1U);
    EXPECT_EQ(moves[0], pv_move);
    EXPECT_EQ(moves[1], tt_move);
}

TEST_F(MovePickerTest, HelperRootRotatesEqualPriorityMoves) {
    TestBoard quiet_board{STARTFEN};

    const auto main_order   = picked_moves(quiet_board,
                                         killers,
                                         history,
                                         ply,
                                         MovePickerMode::MainSearch,
                                         NULL_MOVE,
                                         NULL_MOVE,
                                         true,
                                         0);
    const auto helper_order = picked_moves(quiet_board,
                                           killers,
                                           history,
                                           ply,
                                           MovePickerMode::MainSearch,
                                           NULL_MOVE,
                                           NULL_MOVE,
                                           true,
                                           1);

    ASSERT_GT(main_order.size(), 1U);
    EXPECT_NE(helper_order, main_order);
    EXPECT_EQ(helper_order.front(), main_order[1]);
}

TEST_F(MovePickerTest, MainThreadAndNonRootKeepGeneratedOrderForEqualPriorityMoves) {
    TestBoard quiet_board{STARTFEN};

    MoveList          generated = generate<ALL_MOVES>(quiet_board);
    std::vector<Move> generated_order(generated.begin(), generated.end());
    const auto        main_order      = picked_moves(quiet_board,
                                         killers,
                                         history,
                                         ply,
                                         MovePickerMode::MainSearch,
                                         NULL_MOVE,
                                         NULL_MOVE,
                                         true,
                                         0);
    const auto        helper_non_root = picked_moves(quiet_board,
                                              killers,
                                              history,
                                              ply,
                                              MovePickerMode::MainSearch,
                                              NULL_MOVE,
                                              NULL_MOVE,
                                              false,
                                              1);

    EXPECT_EQ(main_order, generated_order);
    EXPECT_EQ(helper_non_root, generated_order);
}

TEST_F(MovePickerTest, HelperRootKeepsPVAndHashFirst) {
    Move pv_move = Move(B4, C4);
    Move tt_move = Move(E2, E3);

    const auto moves = picked_moves(
        board, killers, history, ply, MovePickerMode::MainSearch, pv_move, tt_move, true, 1);

    ASSERT_GT(moves.size(), 1U);
    EXPECT_EQ(moves[0], pv_move);
    EXPECT_EQ(moves[1], tt_move);
}

TEST_F(MovePickerTest, HelperRootKeepsHashMoveFirstWithoutPV) {
    TestBoard quiet_board{STARTFEN};
    Move      tt_move = Move(E2, E4);

    const auto moves = picked_moves(quiet_board,
                                    killers,
                                    history,
                                    ply,
                                    MovePickerMode::MainSearch,
                                    NULL_MOVE,
                                    tt_move,
                                    true,
                                    1);

    ASSERT_FALSE(moves.empty());
    EXPECT_EQ(moves[0], tt_move);
}

TEST_F(MovePickerTest, MainSearchKeepsPVHashKillerAndHistoryOrder) {
    Move pv_move     = Move(B4, C4);
    Move tt_move     = Move(E2, E3);
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves =
        picked_moves(board, killers, history, ply, MovePickerMode::MainSearch, pv_move, tt_move);

    ASSERT_GT(moves.size(), 4U);
    EXPECT_EQ(moves[0], pv_move);
    EXPECT_EQ(moves[1], tt_move);
    EXPECT_EQ(moves[2], Move(B4, F4));
    EXPECT_EQ(moves[3], killer_move);
    EXPECT_EQ(moves[4], hist_move);
}

TEST_F(MovePickerTest, HelperRootRotatesEqualPriorityMovesButKeepsPVHashFirst) {
    TestBoard quiet_board{STARTFEN};
    Move      tt_move = Move(E2, E4);

    const auto main_order   = picked_moves(quiet_board,
                                         killers,
                                         history,
                                         ply,
                                         MovePickerMode::MainSearch,
                                         NULL_MOVE,
                                         tt_move,
                                         true,
                                         0);
    const auto helper_order = picked_moves(quiet_board,
                                           killers,
                                           history,
                                           ply,
                                           MovePickerMode::MainSearch,
                                           NULL_MOVE,
                                           tt_move,
                                           true,
                                           1);

    ASSERT_GT(main_order.size(), 2U);
    ASSERT_EQ(main_order.front(), tt_move);
    ASSERT_EQ(helper_order.front(), tt_move);
    EXPECT_NE(helper_order, main_order);
    EXPECT_EQ(helper_order[1], main_order[2]);
}
