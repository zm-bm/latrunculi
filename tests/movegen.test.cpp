#include "movegen.hpp"

#include <gtest/gtest.h>

#include "chess.hpp"
#include "constants.hpp"

TEST(MoveGenTest, All) {
    Chess chess{STARTFEN};
    MoveGenerator<GenType::All> moves{chess};
    EXPECT_EQ(moves.size(), 20) << "should have 20 moves";
}

TEST(MoveGenTest, Captures) {
    Chess chess{POS2};
    MoveGenerator<GenType::Captures> moves{chess};
    EXPECT_EQ(moves.size(), 8) << "should have 8 legal captures";
}

// PerftTest in search.test.cpp used to test movegen correctness
