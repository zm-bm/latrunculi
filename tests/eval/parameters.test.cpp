#include "eval/parameters.hpp"

#include <gtest/gtest.h>

TEST(ParametersTest, PieceScores) {
    EXPECT_EQ(eval::piece(NO_PIECETYPE), eval::TaperedScore::Zero);

    EXPECT_EQ(eval::piece(PAWN), (eval::TaperedScore{100, 223}));
    EXPECT_EQ(eval::piece(KNIGHT), (eval::TaperedScore{648, 643}));
    EXPECT_EQ(eval::piece(BISHOP), (eval::TaperedScore{641, 679}));
    EXPECT_EQ(eval::piece(ROOK), (eval::TaperedScore{935, 1083}));
    EXPECT_EQ(eval::piece(QUEEN), (eval::TaperedScore{2033, 2180}));

    EXPECT_EQ(eval::piece(PAWN, WHITE), eval::pawn);
    EXPECT_EQ(eval::piece(KNIGHT, BLACK), -eval::knight);
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
