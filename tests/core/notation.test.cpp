#include "core/notation.hpp"
#include "core/util.hpp"

#include <string>

#include <gtest/gtest.h>

TEST(NotationTest, MakeSquareFromString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, make_square(to_string(sq)));
    }
}

TEST(NotationTest, SquareToString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(to_string(sq), std::string{to_char(file_of(sq))} + to_char(rank_of(sq)));
    }
}
