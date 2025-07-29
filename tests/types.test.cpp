#include "defs.hpp"
#include "util.hpp"

#include <gtest/gtest.h>

TEST(TypesTest, ColorInvert) {
    EXPECT_EQ(WHITE, ~BLACK);
}

TEST(TypesTest, MakeSquareFromFileRank) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, make_square(file_of(sq), rank_of(sq)));
    }
}

TEST(TypesTests, MakeSquareFromString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, make_square(to_string(sq)));
    }
}

TEST(TypesTest, ToString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(to_string(sq), std::string{to_char(file_of(sq))} + to_char(rank_of(sq)));
    }
}

TEST(TypesTest, SquareArithmetic) {
    EXPECT_EQ(A1 + 1, B1);
    EXPECT_EQ(A1, B1 - 1);
    EXPECT_EQ(A1 + 8, A2);
    EXPECT_EQ(A1, A2 - 8);
}

TEST(TypesTest, FileAndRankArithmetic) {
    EXPECT_EQ(FILE1 + 1, FILE2);
    EXPECT_EQ(FILE1, FILE2 - 1);
    EXPECT_EQ(RANK1 + 1, RANK2);
    EXPECT_EQ(RANK1, RANK2 - 1);
}

TEST(TypesTest, RelativeRank) {
    EXPECT_EQ(relative_rank(A1, WHITE), RANK1);
    EXPECT_EQ(relative_rank(A1, BLACK), RANK8);
    EXPECT_EQ(relative_rank(H8, WHITE), RANK8);
    EXPECT_EQ(relative_rank(H8, BLACK), RANK1);
}

TEST(TypesTest, MakePiece) {
    EXPECT_EQ(make_piece(WHITE, PAWN), W_PAWN);
    EXPECT_EQ(make_piece(WHITE, KNIGHT), W_KNIGHT);
    EXPECT_EQ(make_piece(WHITE, BISHOP), W_BISHOP);
    EXPECT_EQ(make_piece(WHITE, ROOK), W_ROOK);
    EXPECT_EQ(make_piece(WHITE, QUEEN), W_QUEEN);
    EXPECT_EQ(make_piece(WHITE, KING), W_KING);
    EXPECT_EQ(make_piece(BLACK, PAWN), B_PAWN);
    EXPECT_EQ(make_piece(BLACK, KNIGHT), B_KNIGHT);
    EXPECT_EQ(make_piece(BLACK, BISHOP), B_BISHOP);
    EXPECT_EQ(make_piece(BLACK, ROOK), B_ROOK);
    EXPECT_EQ(make_piece(BLACK, QUEEN), B_QUEEN);
    EXPECT_EQ(make_piece(BLACK, KING), B_KING);
}

TEST(TypesTest, PieceTypeOf) {
    EXPECT_EQ(type_of(W_PAWN), PAWN);
    EXPECT_EQ(type_of(W_KNIGHT), KNIGHT);
    EXPECT_EQ(type_of(W_BISHOP), BISHOP);
    EXPECT_EQ(type_of(W_ROOK), ROOK);
    EXPECT_EQ(type_of(W_QUEEN), QUEEN);
    EXPECT_EQ(type_of(W_KING), KING);
    EXPECT_EQ(type_of(B_PAWN), PAWN);
    EXPECT_EQ(type_of(B_KNIGHT), KNIGHT);
    EXPECT_EQ(type_of(B_BISHOP), BISHOP);
    EXPECT_EQ(type_of(B_ROOK), ROOK);
    EXPECT_EQ(type_of(B_QUEEN), QUEEN);
    EXPECT_EQ(type_of(B_KING), KING);
}

TEST(TypesTest, PieceColorOf) {
    EXPECT_EQ(color_of(W_PAWN), WHITE);
    EXPECT_EQ(color_of(W_KNIGHT), WHITE);
    EXPECT_EQ(color_of(W_BISHOP), WHITE);
    EXPECT_EQ(color_of(W_ROOK), WHITE);
    EXPECT_EQ(color_of(W_QUEEN), WHITE);
    EXPECT_EQ(color_of(W_KING), WHITE);
    EXPECT_EQ(color_of(B_PAWN), BLACK);
    EXPECT_EQ(color_of(B_KNIGHT), BLACK);
    EXPECT_EQ(color_of(B_BISHOP), BLACK);
    EXPECT_EQ(color_of(B_ROOK), BLACK);
    EXPECT_EQ(color_of(B_QUEEN), BLACK);
    EXPECT_EQ(color_of(B_KING), BLACK);
}
