#include "core/piece.hpp"

#include <gtest/gtest.h>

TEST(PieceTest, MakePiece) {
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

TEST(PieceTest, PieceTypeOf) {
    EXPECT_EQ(type_of(NO_PIECE), NO_PIECETYPE);
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

TEST(PieceTest, PieceTypeIndexing) {
    EXPECT_EQ(N_PIECETYPES, 7);
    EXPECT_EQ(piece_slots, 6);
    EXPECT_EQ(piece_slot(PAWN), 0);
    EXPECT_EQ(piece_slot(KING), 5);
    EXPECT_FALSE(is_piece_type(NO_PIECETYPE));
    EXPECT_TRUE(is_piece_type(PAWN));
    EXPECT_TRUE(is_piece_type(KING));
}

TEST(PieceTest, PromotionPieceValidation) {
    EXPECT_FALSE(valid_promotion_piece(NO_PIECETYPE));
    EXPECT_FALSE(valid_promotion_piece(PAWN));
    EXPECT_TRUE(valid_promotion_piece(KNIGHT));
    EXPECT_TRUE(valid_promotion_piece(BISHOP));
    EXPECT_TRUE(valid_promotion_piece(ROOK));
    EXPECT_TRUE(valid_promotion_piece(QUEEN));
    EXPECT_FALSE(valid_promotion_piece(KING));
}

TEST(PieceTest, PieceColorOf) {
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
