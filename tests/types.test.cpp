#include "types.hpp"

#include <gtest/gtest.h>

TEST(Types_sqFromCoords, CorrectValues) {
    EXPECT_EQ(makeSquare(FILE1, RANK1), A1);
    EXPECT_EQ(makeSquare(FILE1, RANK8), A8);
    EXPECT_EQ(makeSquare(FILE8, RANK1), H1);
    EXPECT_EQ(makeSquare(FILE8, RANK8), H8);
    EXPECT_EQ(makeSquare(FILE4, RANK4), D4);
}

TEST(Types_sqFromString, CorrectValues) {
    EXPECT_EQ(makeSquare("a1"), A1);
    EXPECT_EQ(makeSquare("a8"), A8);
    EXPECT_EQ(makeSquare("h1"), H1);
    EXPECT_EQ(makeSquare("h8"), H8);
    EXPECT_EQ(makeSquare("d4"), D4);
}

TEST(Types_rankFromSq, CorrectValues) {
    EXPECT_EQ(rankOf(A1), RANK1);
    EXPECT_EQ(rankOf(H1), RANK1);
    EXPECT_EQ(rankOf(A4), RANK4);
    EXPECT_EQ(rankOf(H4), RANK4);
    EXPECT_EQ(rankOf(A8), RANK8);
    EXPECT_EQ(rankOf(H8), RANK8);
}

TEST(Types_fileFromSq, CorrectValues) {
    EXPECT_EQ(fileOf(A1), FILE1);
    EXPECT_EQ(fileOf(A8), FILE1);
    EXPECT_EQ(fileOf(D1), FILE4);
    EXPECT_EQ(fileOf(D8), FILE4);
    EXPECT_EQ(fileOf(H1), FILE8);
    EXPECT_EQ(fileOf(H8), FILE8);
}

TEST(Types_makePiece, CorrectValues) {
    EXPECT_EQ(makePiece(WHITE, PAWN), W_PAWN);
    EXPECT_EQ(makePiece(WHITE, KNIGHT), W_KNIGHT);
    EXPECT_EQ(makePiece(WHITE, BISHOP), W_BISHOP);
    EXPECT_EQ(makePiece(WHITE, ROOK), W_ROOK);
    EXPECT_EQ(makePiece(WHITE, QUEEN), W_QUEEN);
    EXPECT_EQ(makePiece(WHITE, KING), W_KING);
    EXPECT_EQ(makePiece(BLACK, PAWN), B_PAWN);
    EXPECT_EQ(makePiece(BLACK, KNIGHT), B_KNIGHT);
    EXPECT_EQ(makePiece(BLACK, BISHOP), B_BISHOP);
    EXPECT_EQ(makePiece(BLACK, ROOK), B_ROOK);
    EXPECT_EQ(makePiece(BLACK, QUEEN), B_QUEEN);
    EXPECT_EQ(makePiece(BLACK, KING), B_KING);
}

TEST(Types_getPieceType, CorrectValues) {
    EXPECT_EQ(pieceTypeOf(W_PAWN), PAWN);
    EXPECT_EQ(pieceTypeOf(W_KNIGHT), KNIGHT);
    EXPECT_EQ(pieceTypeOf(W_BISHOP), BISHOP);
    EXPECT_EQ(pieceTypeOf(W_ROOK), ROOK);
    EXPECT_EQ(pieceTypeOf(W_QUEEN), QUEEN);
    EXPECT_EQ(pieceTypeOf(W_KING), KING);
    EXPECT_EQ(pieceTypeOf(B_PAWN), PAWN);
    EXPECT_EQ(pieceTypeOf(B_KNIGHT), KNIGHT);
    EXPECT_EQ(pieceTypeOf(B_BISHOP), BISHOP);
    EXPECT_EQ(pieceTypeOf(B_ROOK), ROOK);
    EXPECT_EQ(pieceTypeOf(B_QUEEN), QUEEN);
    EXPECT_EQ(pieceTypeOf(B_KING), KING);
}

TEST(Types_getPieceColor, CorrectValues) {
    EXPECT_EQ(pieceColorOf(W_PAWN), WHITE);
    EXPECT_EQ(pieceColorOf(W_KNIGHT), WHITE);
    EXPECT_EQ(pieceColorOf(W_BISHOP), WHITE);
    EXPECT_EQ(pieceColorOf(W_ROOK), WHITE);
    EXPECT_EQ(pieceColorOf(W_QUEEN), WHITE);
    EXPECT_EQ(pieceColorOf(W_KING), WHITE);
    EXPECT_EQ(pieceColorOf(B_PAWN), BLACK);
    EXPECT_EQ(pieceColorOf(B_KNIGHT), BLACK);
    EXPECT_EQ(pieceColorOf(B_BISHOP), BLACK);
    EXPECT_EQ(pieceColorOf(B_ROOK), BLACK);
    EXPECT_EQ(pieceColorOf(B_QUEEN), BLACK);
    EXPECT_EQ(pieceColorOf(B_KING), BLACK);
}

TEST(Types_pawnMovePush, CorrectValues) {
    Square sq = pawnMove<WHITE, PUSH, true>(E4);
    EXPECT_EQ(sq, E5);
    sq = pawnMove<WHITE, PUSH, false>(E5);
    EXPECT_EQ(sq, E4);

    sq = pawnMove<BLACK, PUSH, true>(E5);
    EXPECT_EQ(sq, E4);
    sq = pawnMove<BLACK, PUSH, false>(E4);
    EXPECT_EQ(sq, E5);
}

TEST(Types_pawnMoveRight, CorrectValues) {
    Square sq = pawnMove<WHITE, RIGHT, true>(D4);
    EXPECT_EQ(sq, E5);
    sq = pawnMove<WHITE, RIGHT, false>(E5);
    EXPECT_EQ(sq, D4);

    sq = pawnMove<BLACK, RIGHT, true>(E5);
    EXPECT_EQ(sq, D4);
    sq = pawnMove<BLACK, RIGHT, false>(D4);
    EXPECT_EQ(sq, E5);
}

TEST(Types_pawnMoveLeft, CorrectValues) {
    Square sq;
    sq = pawnMove<WHITE, LEFT, true>(E4);
    EXPECT_EQ(sq, D5);
    sq = pawnMove<WHITE, LEFT, false>(D5);
    EXPECT_EQ(sq, E4);

    sq = pawnMove<BLACK, LEFT, true>(D5);
    EXPECT_EQ(sq, E4);
    sq = pawnMove<BLACK, LEFT, false>(E4);
    EXPECT_EQ(sq, D5);
}


TEST(Types_Color, InvertFlipsColor) { EXPECT_EQ(WHITE, ~BLACK); }

TEST(Types_Square, Arithmetic) {
    EXPECT_EQ(A1 + 1, B1);
    EXPECT_EQ(A1, B1 - 1);
    EXPECT_EQ(A1 + 8, A2);
    EXPECT_EQ(A1, A2 - 8);
}

TEST(Types_File, Arithmetic) {
    EXPECT_EQ(FILE1 + 1, FILE2);
    EXPECT_EQ(FILE1, FILE2 - 1);
}

TEST(Types_Rank, Arithmetic) {
    EXPECT_EQ(RANK1 + 1, RANK2);
    EXPECT_EQ(RANK1, RANK2 - 1);
}

TEST(Types_CastleRights, LogicOperators) {
    EXPECT_EQ(ALL_CASTLE & WHITE_CASTLE, WHITE_CASTLE);
    EXPECT_EQ(ALL_CASTLE ^ WHITE_CASTLE, BLACK_CASTLE);
    EXPECT_EQ(ALL_CASTLE ^ WHITE_OOO, BLACK_CASTLE | WHITE_OO);
    EXPECT_EQ(ALL_CASTLE ^ WHITE_OO, BLACK_CASTLE | WHITE_OOO);
    EXPECT_EQ(ALL_CASTLE & BLACK_CASTLE, BLACK_CASTLE);
    EXPECT_EQ(ALL_CASTLE ^ BLACK_CASTLE, WHITE_CASTLE);
    EXPECT_EQ(ALL_CASTLE ^ BLACK_OOO, WHITE_CASTLE | BLACK_OO);
    EXPECT_EQ(ALL_CASTLE ^ BLACK_OO, WHITE_CASTLE | BLACK_OOO);
}
