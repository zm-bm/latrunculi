#include "eval/parameters.hpp"

#include <gtest/gtest.h>

TEST(ParametersTest, MapsPieceTypesAndColors) {
    constexpr eval::TaperedScore values[] = {
        eval::TaperedScore::Zero,
        eval::pawn,
        eval::knight,
        eval::bishop,
        eval::rook,
        eval::queen,
        eval::TaperedScore::Zero,
    };

    for (PieceType type = NO_PIECETYPE; type <= KING; type = PieceType(type + 1)) {
        EXPECT_EQ(eval::piece(type, WHITE), values[type]);
        EXPECT_EQ(eval::piece(type, BLACK), -values[type]);
    }
}

TEST(ParametersTest, PieceSquareScores) {
    for (Square sq = A1; sq <= H8; ++sq) {
        Square bsq = Square(sq ^ 63); // flip square for black
        EXPECT_EQ(eval::piece_sq(PAWN, WHITE, sq), -eval::piece_sq(PAWN, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(KNIGHT, WHITE, sq), -eval::piece_sq(KNIGHT, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(BISHOP, WHITE, sq), -eval::piece_sq(BISHOP, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(ROOK, WHITE, sq), -eval::piece_sq(ROOK, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(QUEEN, WHITE, sq), -eval::piece_sq(QUEEN, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(KING, WHITE, sq), -eval::piece_sq(KING, BLACK, bsq));
    }
}
