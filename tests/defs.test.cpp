#include "defs.hpp"

#include <gtest/gtest.h>

TEST(Defs_sqFromCoords, CorrectValues) {
    EXPECT_EQ(Defs::sqFromCoords(FILE1, RANK1), A1);
    EXPECT_EQ(Defs::sqFromCoords(FILE1, RANK8), A8);
    EXPECT_EQ(Defs::sqFromCoords(FILE8, RANK1), H1);
    EXPECT_EQ(Defs::sqFromCoords(FILE8, RANK8), H8);
    EXPECT_EQ(Defs::sqFromCoords(FILE4, RANK4), D4);
}

TEST(Defs_sqFromString, CorrectValues) {
    EXPECT_EQ(Defs::sqFromString("a1"), A1);
    EXPECT_EQ(Defs::sqFromString("a8"), A8);
    EXPECT_EQ(Defs::sqFromString("h1"), H1);
    EXPECT_EQ(Defs::sqFromString("h8"), H8);
    EXPECT_EQ(Defs::sqFromString("d4"), D4);
}

TEST(Types_rankFromSq, CorrectValues) {
    EXPECT_EQ(Defs::rankFromSq(A1), RANK1);
    EXPECT_EQ(Defs::rankFromSq(H1), RANK1);
    EXPECT_EQ(Defs::rankFromSq(A4), RANK4);
    EXPECT_EQ(Defs::rankFromSq(H4), RANK4);
    EXPECT_EQ(Defs::rankFromSq(A8), RANK8);
    EXPECT_EQ(Defs::rankFromSq(H8), RANK8);
}

TEST(Types_fileFromSq, CorrectValues) {
    EXPECT_EQ(Defs::fileFromSq(A1), FILE1);
    EXPECT_EQ(Defs::fileFromSq(A8), FILE1);
    EXPECT_EQ(Defs::fileFromSq(D1), FILE4);
    EXPECT_EQ(Defs::fileFromSq(D8), FILE4);
    EXPECT_EQ(Defs::fileFromSq(H1), FILE8);
    EXPECT_EQ(Defs::fileFromSq(H8), FILE8);
}

TEST(Defs_makePiece, CorrectValues) {
    EXPECT_EQ(Defs::makePiece(WHITE, PAWN), W_PAWN);
    EXPECT_EQ(Defs::makePiece(WHITE, KNIGHT), W_KNIGHT);
    EXPECT_EQ(Defs::makePiece(WHITE, BISHOP), W_BISHOP);
    EXPECT_EQ(Defs::makePiece(WHITE, ROOK), W_ROOK);
    EXPECT_EQ(Defs::makePiece(WHITE, QUEEN), W_QUEEN);
    EXPECT_EQ(Defs::makePiece(WHITE, KING), W_KING);
    EXPECT_EQ(Defs::makePiece(BLACK, PAWN), B_PAWN);
    EXPECT_EQ(Defs::makePiece(BLACK, KNIGHT), B_KNIGHT);
    EXPECT_EQ(Defs::makePiece(BLACK, BISHOP), B_BISHOP);
    EXPECT_EQ(Defs::makePiece(BLACK, ROOK), B_ROOK);
    EXPECT_EQ(Defs::makePiece(BLACK, QUEEN), B_QUEEN);
    EXPECT_EQ(Defs::makePiece(BLACK, KING), B_KING);
}

TEST(Defs_getPieceType, CorrectValues) {
    EXPECT_EQ(Defs::getPieceType(W_PAWN), PAWN);
    EXPECT_EQ(Defs::getPieceType(W_KNIGHT), KNIGHT);
    EXPECT_EQ(Defs::getPieceType(W_BISHOP), BISHOP);
    EXPECT_EQ(Defs::getPieceType(W_ROOK), ROOK);
    EXPECT_EQ(Defs::getPieceType(W_QUEEN), QUEEN);
    EXPECT_EQ(Defs::getPieceType(W_KING), KING);
    EXPECT_EQ(Defs::getPieceType(B_PAWN), PAWN);
    EXPECT_EQ(Defs::getPieceType(B_KNIGHT), KNIGHT);
    EXPECT_EQ(Defs::getPieceType(B_BISHOP), BISHOP);
    EXPECT_EQ(Defs::getPieceType(B_ROOK), ROOK);
    EXPECT_EQ(Defs::getPieceType(B_QUEEN), QUEEN);
    EXPECT_EQ(Defs::getPieceType(B_KING), KING);
}

TEST(Defs_getPieceColor, CorrectValues) {
    EXPECT_EQ(Defs::getPieceColor(W_PAWN), WHITE);
    EXPECT_EQ(Defs::getPieceColor(W_KNIGHT), WHITE);
    EXPECT_EQ(Defs::getPieceColor(W_BISHOP), WHITE);
    EXPECT_EQ(Defs::getPieceColor(W_ROOK), WHITE);
    EXPECT_EQ(Defs::getPieceColor(W_QUEEN), WHITE);
    EXPECT_EQ(Defs::getPieceColor(W_KING), WHITE);
    EXPECT_EQ(Defs::getPieceColor(B_PAWN), BLACK);
    EXPECT_EQ(Defs::getPieceColor(B_KNIGHT), BLACK);
    EXPECT_EQ(Defs::getPieceColor(B_BISHOP), BLACK);
    EXPECT_EQ(Defs::getPieceColor(B_ROOK), BLACK);
    EXPECT_EQ(Defs::getPieceColor(B_QUEEN), BLACK);
    EXPECT_EQ(Defs::getPieceColor(B_KING), BLACK);
}

TEST(Defs_pawnMovePush, CorrectValues) {
    Square sq = Defs::pawnMove<WHITE, PUSH, true>(E4);
    EXPECT_EQ(sq, E5);
    sq = Defs::pawnMove<WHITE, PUSH, false>(E5);
    EXPECT_EQ(sq, E4);

    sq = Defs::pawnMove<BLACK, PUSH, true>(E5);
    EXPECT_EQ(sq, E4);
    sq = Defs::pawnMove<BLACK, PUSH, false>(E4);
    EXPECT_EQ(sq, E5);
}

TEST(Defs_pawnMoveRight, CorrectValues) {
    Square sq = Defs::pawnMove<WHITE, RIGHT, true>(D4);
    EXPECT_EQ(sq, E5);
    sq = Defs::pawnMove<WHITE, RIGHT, false>(E5);
    EXPECT_EQ(sq, D4);

    sq = Defs::pawnMove<BLACK, RIGHT, true>(E5);
    EXPECT_EQ(sq, D4);
    sq = Defs::pawnMove<BLACK, RIGHT, false>(D4);
    EXPECT_EQ(sq, E5);
}

TEST(Defs_pawnMoveLeft, CorrectValues) {
    Square sq;
    sq = Defs::pawnMove<WHITE, LEFT, true>(E4);
    EXPECT_EQ(sq, D5);
    sq = Defs::pawnMove<WHITE, LEFT, false>(D5);
    EXPECT_EQ(sq, E4);

    sq = Defs::pawnMove<BLACK, LEFT, true>(D5);
    EXPECT_EQ(sq, E4);
    sq = Defs::pawnMove<BLACK, LEFT, false>(E4);
    EXPECT_EQ(sq, D5);
}

TEST(Defs_split, ValidInputs) {
    std::string input = "a,b,c";
    std::vector<std::string> expected = {"a", "b", "c"};
    EXPECT_EQ(Defs::split(input, ','), expected);
}

TEST(Defs_split, ConsecutiveDelimiters) {
    std::string input = "a,,b,c";
    char delimiter = ',';
    std::vector<std::string> expected = {"a", "", "b", "c"};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}

TEST(Defs_split, WithoutDelimiter) {
    std::string input = "abc";
    char delimiter = ',';
    std::vector<std::string> expected = {"abc"};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}

TEST(Defs_split, EmptyString) {
    std::string input = "";
    char delimiter = ',';
    std::vector<std::string> expected = {};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}

TEST(Defs_split, SpecialCharacters) {
    std::string input = "a!b@c#d$";
    char delimiter = '@';
    std::vector<std::string> expected = {"a!b", "c#d$"};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}
