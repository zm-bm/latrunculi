#include "eval.hpp"
#include <gtest/gtest.h>

TEST(EvalTest, PieceScores) {
    EXPECT_EQ(eval::piece(PAWN), eval::pawn);
    EXPECT_EQ(eval::piece(KNIGHT), eval::knight);
    EXPECT_EQ(eval::piece(BISHOP), eval::bishop);
    EXPECT_EQ(eval::piece(ROOK), eval::rook);
    EXPECT_EQ(eval::piece(QUEEN), eval::queen);

    EXPECT_EQ(eval::piece(PAWN, WHITE), eval::pawn);
    EXPECT_EQ(eval::piece(KNIGHT, BLACK), -eval::knight);
}

TEST(EvalTest, PieceSquareScores) {
    for (Square sq = A1; sq <= H8; ++sq) {
        Square bsq = Square(sq ^ 63); // flip square for black
        EXPECT_EQ(eval::piece_sq(PAWN, WHITE, sq), -eval::piece_sq(PAWN, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(KNIGHT, WHITE, sq), -eval::piece_sq(KNIGHT, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(BISHOP, WHITE, sq), -eval::piece_sq(BISHOP, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(ROOK, WHITE, sq), -eval::piece_sq(ROOK, BLACK, bsq));
        EXPECT_EQ(eval::piece_sq(QUEEN, WHITE, sq), -eval::piece_sq(QUEEN, BLACK, bsq));
    }
}
