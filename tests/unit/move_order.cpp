#include "move_order.hpp"

#include <gtest/gtest.h>

#include "board.hpp"
#include "heuristics.hpp"
#include "movegen.hpp"

class MoveOrderTest : public ::testing::Test {
   protected:
    void SetUp() override { board.loadFEN(POS3); }

    int ply = 5;
    Board board;
    Heuristics heuristics;
};

TEST_F(MoveOrderTest, OrderMoves) {
    MoveGenerator<GenType::All> moves(board);
    MoveOrder moveOrder(board, heuristics, ply);
    moves.sort(moveOrder);

    EXPECT_FALSE(moves.empty()) << "Move list should not be empty";
}

TEST_F(MoveOrderTest, OrderHeuristicMoves) {
    Move killerMove = Move(A5, A4);
    heuristics.killers.update(killerMove, ply);

    Move historyMove = Move(A5, A6);
    heuristics.history.update(board.sideToMove(), historyMove.from(), historyMove.to(), ply);

    MoveGenerator<GenType::All> moves(board);
    MoveOrder moveOrder(board, heuristics, ply);
    moves.sort(moveOrder);

    EXPECT_GT(moves.size(), 3) << "Should be more than 3 moves";
    EXPECT_EQ(moves[0], Move(B4, F4)) << "First move should be only good capture";
    EXPECT_EQ(moves[1], killerMove) << "Second move should be killer move";
    EXPECT_EQ(moves[2], historyMove) << "Third move should be history move";
}

TEST_F(MoveOrderTest, Hash_PVMovesFirst) {
    Move pvMove   = Move(B4, C4);
    Move hashMove = Move(E2, E3);

    MoveGenerator<GenType::All> moves(board);
    MoveOrder moveOrder(board, heuristics, ply, pvMove, hashMove);
    moves.sort(moveOrder);

    EXPECT_GT(moves.size(), 1);
    EXPECT_EQ(moves[0], hashMove) << "Hash move is not ordered first.";
    EXPECT_EQ(moves[1], pvMove) << "PV move is not ordered second.";
}
