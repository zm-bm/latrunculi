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

TEST(BaseTest, IsMate) {
    EXPECT_TRUE(isMate(MATE_BOUND + 1));
    EXPECT_TRUE(isMate(-(MATE_BOUND + 1)));
    EXPECT_FALSE(isMate(500));
}

TEST(BaseTest, MateDistance) {
    int mateIn5 = MATE_VALUE - 10;
    EXPECT_EQ(mateDistance(mateIn5), 10);
    int matedIn10 = -(MATE_VALUE - 20);
    EXPECT_EQ(mateDistance(matedIn10), 20);
}

TEST(BaseTest, PawnMove) {
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Push, FORWARD>(E4)), E5);
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Push, BACKWARD>(E5)), E4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Push, FORWARD>(E5)), E4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Push, BACKWARD>(E4)), E5);

    EXPECT_EQ((pawnMove<WHITE, PawnMove::Right, FORWARD>(D4)), E5);
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Right, BACKWARD>(E5)), D4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Right, FORWARD>(E5)), D4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Right, BACKWARD>(D4)), E5);

    EXPECT_EQ((pawnMove<WHITE, PawnMove::Left, FORWARD>(E4)), D5);
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Left, BACKWARD>(D5)), E4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Left, FORWARD>(D5)), E4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Left, BACKWARD>(E4)), D5);

    EXPECT_EQ((pawnMove<WHITE, PawnMove::Double, FORWARD>(E2)), E4);
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Double, BACKWARD>(E4)), E2);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Double, FORWARD>(D7)), D5);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Double, BACKWARD>(D5)), D7);
}