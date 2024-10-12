#include "gtest/gtest.h"
#include "movegen.hpp"
#include "chess.hpp"
#include "globals.hpp"

TEST(MoveGenTest, TestInitialPositionMoves) {
    Chess chess{G::STARTFEN};
    MoveGenerator moveGen{&chess};
    moveGen.generatePseudoLegalMoves();
    EXPECT_EQ(moveGen.moves.size(), 20) << "Initial position should have 20 legal moves.";
}
