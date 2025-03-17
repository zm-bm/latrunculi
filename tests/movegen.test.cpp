#include "movegen.hpp"

#include <gtest/gtest.h>

#include "board.hpp"
#include "constants.hpp"

TEST(MoveGenTest, All) {
    Board board{STARTFEN};
    MoveGenerator<GenType::All> moves{board};
    EXPECT_EQ(moves.size(), 20) << "should have 20 moves";
}

TEST(MoveGenTest, Captures) {
    Board board{POS2};
    MoveGenerator<GenType::Captures> moves{board};
    EXPECT_EQ(moves.size(), 8) << "should have 8 legal captures";
}

// PerftTest in search.test.cpp used to test movegen correctness
