#include "movegen.hpp"

#include <gtest/gtest.h>

#include "chess.hpp"
#include "constants.hpp"

TEST(MoveGenTest, GeneratePseudoLegalMoves) {
    Chess chess{LtrnConsts::STARTFEN};
    MoveGenerator moveGen{&chess};
    moveGen.generatePseudoLegalMoves();
    EXPECT_EQ(moveGen.moves.size(), 20) << "should have 20 moves";
}

TEST(MoveGenTest, GenerateCaptures) {
    Chess chess{LtrnConsts::POS2};
    MoveGenerator moveGen{&chess};
    moveGen.generateCaptures();
    EXPECT_EQ(moveGen.moves.size(), 8) << "should have 8 legal captures";
}

// PerftTest in search.test.cpp used to test movegen correctness
