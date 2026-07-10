#include "core/notation.hpp"
#include "core/bitboard.hpp"

#include <sstream>
#include <string>

#include <gtest/gtest.h>

TEST(NotationTest, MakeSquareFromString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, parse_square(to_string(sq)));
    }
}

TEST(NotationTest, SquareToString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(to_string(sq),
                  std::string{to_char(square::file_of(sq))} + to_char(square::rank_of(sq)));
    }
}

TEST(NotationTest, BitboardDebugToString) {
    std::ostringstream out;

    out << debug_bitboard(bb::set(A1, H8));

    EXPECT_EQ(out.str(),
              " .......1 8\n"
              " ........ 7\n"
              " ........ 6\n"
              " ........ 5\n"
              " ........ 4\n"
              " ........ 3\n"
              " ........ 2\n"
              " 1....... 1\n"
              " abcdefgh\n");
}
