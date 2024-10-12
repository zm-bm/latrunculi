#include "gtest/gtest.h"
#include "movegen.hpp"
#include "chess.hpp"
#include "globals.hpp"

TEST(MoveGenTest, GeneratePseudoLegalMoves) {
    Chess chess{G::STARTFEN};
    MoveGenerator moveGen{&chess};
    moveGen.generatePseudoLegalMoves();
    EXPECT_EQ(moveGen.moves.size(), 20) << "Initial position should have 20 legal moves.";
}

TEST(MoveGenTest, GenerateCaptures) {
    Chess chess{G::POS2};
    MoveGenerator moveGen{&chess};
    moveGen.generateCaptures();
    EXPECT_EQ(moveGen.moves.size(), 8) << "Kiwipete position should have 8 legal captures.";
}

// PerftTest in search.test.cpp used to test movegen correctness
