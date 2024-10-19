#include "defs.hpp"

#include <gtest/gtest.h>

TEST(DefsTest, sqFromCoords) {
    EXPECT_EQ(Defs::sqFromCoords(FILE1, RANK1), A1);
    EXPECT_EQ(Defs::sqFromCoords(FILE1, RANK8), A8);
    EXPECT_EQ(Defs::sqFromCoords(FILE8, RANK1), H1);
    EXPECT_EQ(Defs::sqFromCoords(FILE8, RANK8), H8);
    EXPECT_EQ(Defs::sqFromCoords(FILE4, RANK4), D4);
}

TEST(DefsTest, sqFromString) {
    EXPECT_EQ(Defs::sqFromString("a1"), A1);
    EXPECT_EQ(Defs::sqFromString("a8"), A8);
    EXPECT_EQ(Defs::sqFromString("h1"), H1);
    EXPECT_EQ(Defs::sqFromString("h8"), H8);
    EXPECT_EQ(Defs::sqFromString("d4"), D4);
}

TEST(TypesTest, rankFromSq) {
    EXPECT_EQ(Defs::rankFromSq(A1), RANK1);
    EXPECT_EQ(Defs::rankFromSq(H1), RANK1);
    EXPECT_EQ(Defs::rankFromSq(A4), RANK4);
    EXPECT_EQ(Defs::rankFromSq(H4), RANK4);
    EXPECT_EQ(Defs::rankFromSq(A8), RANK8);
    EXPECT_EQ(Defs::rankFromSq(H8), RANK8);
}

TEST(TypesTest, fileFromSq) {
    EXPECT_EQ(Defs::fileFromSq(A1), FILE1);
    EXPECT_EQ(Defs::fileFromSq(A8), FILE1);
    EXPECT_EQ(Defs::fileFromSq(D1), FILE4);
    EXPECT_EQ(Defs::fileFromSq(D8), FILE4);
    EXPECT_EQ(Defs::fileFromSq(H1), FILE8);
    EXPECT_EQ(Defs::fileFromSq(H8), FILE8);
}

TEST(DefsTest, makePiece) {
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

TEST(DefsTest, getPieceRole) {
    EXPECT_EQ(Defs::getPieceRole(W_PAWN), PAWN);
    EXPECT_EQ(Defs::getPieceRole(W_KNIGHT), KNIGHT);
    EXPECT_EQ(Defs::getPieceRole(W_BISHOP), BISHOP);
    EXPECT_EQ(Defs::getPieceRole(W_ROOK), ROOK);
    EXPECT_EQ(Defs::getPieceRole(W_QUEEN), QUEEN);
    EXPECT_EQ(Defs::getPieceRole(W_KING), KING);
    EXPECT_EQ(Defs::getPieceRole(B_PAWN), PAWN);
    EXPECT_EQ(Defs::getPieceRole(B_KNIGHT), KNIGHT);
    EXPECT_EQ(Defs::getPieceRole(B_BISHOP), BISHOP);
    EXPECT_EQ(Defs::getPieceRole(B_ROOK), ROOK);
    EXPECT_EQ(Defs::getPieceRole(B_QUEEN), QUEEN);
    EXPECT_EQ(Defs::getPieceRole(B_KING), KING);
}

TEST(DefsTest, getPieceColor) {
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

TEST(DefsTest, pawnMovePush) {
    Square sq = Defs::pawnMove<WHITE, PawnMove::PUSH, true>(E4);
    EXPECT_EQ(sq, E5);
    sq = Defs::pawnMove<WHITE, PawnMove::PUSH, false>(E5);
    EXPECT_EQ(sq, E4);

    sq = Defs::pawnMove<BLACK, PawnMove::PUSH, true>(E5);
    EXPECT_EQ(sq, E4);
    sq = Defs::pawnMove<BLACK, PawnMove::PUSH, false>(E4);
    EXPECT_EQ(sq, E5);
}

TEST(DefsTest, pawnMoveRight) {
    Square sq = Defs::pawnMove<WHITE, PawnMove::RIGHT, true>(D4);
    EXPECT_EQ(sq, E5);
    sq = Defs::pawnMove<WHITE, PawnMove::RIGHT, false>(E5);
    EXPECT_EQ(sq, D4);

    sq = Defs::pawnMove<BLACK, PawnMove::RIGHT, true>(E5);
    EXPECT_EQ(sq, D4);
    sq = Defs::pawnMove<BLACK, PawnMove::RIGHT, false>(D4);
    EXPECT_EQ(sq, E5);
}

TEST(DefsTest, pawnMoveLeft) {
    Square sq = Defs::pawnMove<WHITE, PawnMove::LEFT, true>(E4);
    EXPECT_EQ(sq, D5);
    sq = Defs::pawnMove<WHITE, PawnMove::LEFT, false>(D5);
    EXPECT_EQ(sq, E4);

    sq = Defs::pawnMove<BLACK, PawnMove::LEFT, true>(D5);
    EXPECT_EQ(sq, E4);
    sq = Defs::pawnMove<BLACK, PawnMove::LEFT, false>(E4);
    EXPECT_EQ(sq, D5);
}

TEST(DefsTest, split) {
    std::string input = "a,b,c";
    char delimiter = ',';
    std::vector<std::string> expected = {"a", "b", "c"};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}

TEST(DefsTest, splitWithConsecutiveDelimiters) {
    std::string input = "a,,b,c";
    char delimiter = ',';
    std::vector<std::string> expected = {"a", "", "b", "c"};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}

TEST(DefsTest, splitWithoutDelimiter) {
    std::string input = "abc";
    char delimiter = ',';
    std::vector<std::string> expected = {"abc"};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}

TEST(DefsTest, splitEmptyString) {
    std::string input = "";
    char delimiter = ',';
    std::vector<std::string> expected = {};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}

TEST(DefsTest, splitWithSpecialCharacters) {
    std::string input = "a!b@c#d$";
    char delimiter = '@';
    std::vector<std::string> expected = {"a!b", "c#d$"};

    EXPECT_EQ(Defs::split(input, delimiter), expected);
}
