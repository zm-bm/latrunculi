#include "base.hpp"

#include "constants.hpp"
#include "gtest/gtest.h"
#include "score.hpp"
#include "types.hpp"

TEST(BaseTest, PieceValue) {
    int pawnValue = pieceValue(PieceType::Pawn);
    EXPECT_EQ(pawnValue, PAWN_VALUE_MG);
}

TEST(BaseTest, PieceScore) {
    auto pawnScore = pieceScore(PieceType::Pawn);
    EXPECT_EQ(pawnScore.mg, PAWN_VALUE_MG);
    EXPECT_EQ(pawnScore.eg, PAWN_VALUE_EG);
}

TEST(BaseTest, PieceScoreColor) {
    auto whitePawnScore = pieceScore(PieceType::Pawn, WHITE);
    EXPECT_EQ(whitePawnScore.mg, PAWN_VALUE_MG);
    EXPECT_EQ(whitePawnScore.eg, PAWN_VALUE_EG);

    auto blackPawnScore = pieceScore(PieceType::Pawn, BLACK);
    EXPECT_EQ(blackPawnScore.mg, -PAWN_VALUE_MG);
    EXPECT_EQ(blackPawnScore.eg, -PAWN_VALUE_EG);
}

TEST(BaseTest, PieceSqScore) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        auto whiteScore = pieceSqScore(PieceType::Pawn, WHITE, sq);
        auto blackScore = pieceSqScore(PieceType::Pawn, BLACK, Square(H8 - int(sq)));
        EXPECT_EQ(whiteScore, -blackScore);
    }
}

TEST(BaseTest, IsMateScore) {
    EXPECT_TRUE(isMateScore(MATE_IN_MAX_PLY + 1));
    EXPECT_TRUE(isMateScore(-(MATE_IN_MAX_PLY + 1)));
    EXPECT_FALSE(isMateScore(500));
}

TEST(BaseTest, MateDistance) {
    int scorePos = MATE_SCORE - 10;
    EXPECT_EQ(mateDistance(scorePos), 10);
    int scoreNeg = -(MATE_SCORE - 20);
    EXPECT_EQ(mateDistance(scoreNeg), 20);
}

TEST(BaseTest, TTScore) {
    int ply = 3;

    int mateInFive = MATE_SCORE - 5;
    EXPECT_EQ(ttScore(mateInFive, ply), mateInFive + ply);

    int matedInSix = -(MATE_SCORE - 6);
    EXPECT_EQ(ttScore(matedInSix, ply), matedInSix - ply);
}
