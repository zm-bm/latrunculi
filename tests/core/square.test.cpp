#include "core/square.hpp"

#include <array>

#include "core/bitboard.hpp"

#include <gtest/gtest.h>

TEST(SquareTest, MakeSquareFromFileRank) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, square::make(square::file_of(sq), square::rank_of(sq)));
    }
}

TEST(SquareTest, SquareArithmetic) {
    EXPECT_EQ(A1 + 1, B1);
    EXPECT_EQ(A1, B1 - 1);
    EXPECT_EQ(A1 + 8, A2);
    EXPECT_EQ(A1, A2 - 8);
}

TEST(SquareTest, FileAndRankArithmetic) {
    EXPECT_EQ(FILE1 + 1, FILE2);
    EXPECT_EQ(FILE1, FILE2 - 1);
    EXPECT_EQ(RANK1 + 1, RANK2);
    EXPECT_EQ(RANK1, RANK2 - 1);
}

TEST(SquareTest, RelativeSquare) {
    EXPECT_EQ(square::relative(A1, WHITE), A1);
    EXPECT_EQ(square::relative(H8, WHITE), H8);
    EXPECT_EQ(square::relative(A1, BLACK), H8);
    EXPECT_EQ(square::relative(H8, BLACK), A1);
}

TEST(SquareTest, RelativeRank) {
    EXPECT_EQ(square::relative_rank(A1, WHITE), RANK1);
    EXPECT_EQ(square::relative_rank(A1, BLACK), RANK8);
    EXPECT_EQ(square::relative_rank(H8, WHITE), RANK8);
    EXPECT_EQ(square::relative_rank(H8, BLACK), RANK1);
}

TEST(SquareTest, Distance) {
    EXPECT_EQ(square::distance(A1, A1), 0);
    EXPECT_EQ(square::distance(A1, A2), 1);
    EXPECT_EQ(square::distance(A1, B1), 1);
    EXPECT_EQ(square::distance(A1, B2), 1);
    EXPECT_EQ(square::distance(A1, G7), 6);
    EXPECT_EQ(square::distance(A1, H7), 7);
    EXPECT_EQ(square::distance(A1, G8), 7);
    EXPECT_EQ(square::distance(A1, H8), 7);
}

TEST(SquareTest, RayMasks) {
    struct Case {
        Square   from;
        Square   to;
        Bitboard collinear;
        Bitboard between;
    };
    constexpr std::array cases = {
        Case{B2, D2, bb::rank(RANK2), bb::set(C2)},
        Case{B2, B4, bb::file(FILE2), bb::set(B3)},
        Case{A1, H8, bb::set(A1, B2, C3, D4, E5, F6, G7, H8), bb::set(B2, C3, D4, E5, F6, G7)},
        Case{A8, H1, bb::set(A8, B7, C6, D5, E4, F3, G2, H1), bb::set(B7, C6, D5, E4, F3, G2)},
        Case{C1, A3, bb::set(A3, B2, C1), bb::set(B2)},
        Case{B2, C4, 0, 0},
    };

    for (const auto& test : cases) {
        EXPECT_EQ(square::collinear(test.from, test.to), test.collinear);
        EXPECT_EQ(square::collinear(test.to, test.from), test.collinear);
        EXPECT_EQ(square::between(test.from, test.to), test.between);
        EXPECT_EQ(square::between(test.to, test.from), test.between);
    }
}
