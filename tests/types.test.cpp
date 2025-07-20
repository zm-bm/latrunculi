#include "types.hpp"

#include <gtest/gtest.h>

TEST(TypesTest, ColorInvert) { EXPECT_EQ(WHITE, ~BLACK); }

TEST(TypesTest, MakeSquareFromFileRank) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, makeSquare(fileOf(sq), rankOf(sq)));
    }
}

TEST(TypesTests, MakeSquareFromString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, makeSquare(toString(sq)));
    }
}

TEST(TypesTest, ToString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(toString(sq), std::string{toChar(fileOf(sq))} + toChar(rankOf(sq)));
    }
}

TEST(TypesTest, SquareArithmetic) {
    EXPECT_EQ(A1 + 1, B1);
    EXPECT_EQ(A1, B1 - 1);
    EXPECT_EQ(A1 + 8, A2);
    EXPECT_EQ(A1, A2 - 8);
}

TEST(TypesTest, FileAndRankArithmetic) {
    EXPECT_EQ(File::F1 + 1, File::F2);
    EXPECT_EQ(File::F1, File::F2 - 1);
    EXPECT_EQ(Rank::R1 + 1, Rank::R2);
    EXPECT_EQ(Rank::R1, Rank::R2 - 1);
}

TEST(TypesTest, RelativeRank) {
    EXPECT_EQ(relativeRank(A1, WHITE), Rank::R1);
    EXPECT_EQ(relativeRank(A1, BLACK), Rank::R8);
    EXPECT_EQ(relativeRank(H8, WHITE), Rank::R8);
    EXPECT_EQ(relativeRank(H8, BLACK), Rank::R1);
}

TEST(TypesTest, MakePiece) {
    EXPECT_EQ(makePiece(WHITE, PieceType::Pawn), Piece::WPawn);
    EXPECT_EQ(makePiece(WHITE, PieceType::Knight), Piece::WKnight);
    EXPECT_EQ(makePiece(WHITE, PieceType::Bishop), Piece::WBishop);
    EXPECT_EQ(makePiece(WHITE, PieceType::Rook), Piece::WRook);
    EXPECT_EQ(makePiece(WHITE, PieceType::Queen), Piece::WQueen);
    EXPECT_EQ(makePiece(WHITE, PieceType::King), Piece::WKing);
    EXPECT_EQ(makePiece(BLACK, PieceType::Pawn), Piece::BPawn);
    EXPECT_EQ(makePiece(BLACK, PieceType::Knight), Piece::BKnight);
    EXPECT_EQ(makePiece(BLACK, PieceType::Bishop), Piece::BBishop);
    EXPECT_EQ(makePiece(BLACK, PieceType::Rook), Piece::BRook);
    EXPECT_EQ(makePiece(BLACK, PieceType::Queen), Piece::BQueen);
    EXPECT_EQ(makePiece(BLACK, PieceType::King), Piece::BKing);
}

TEST(TypesTest, PieceTypeOf) {
    EXPECT_EQ(pieceTypeOf(Piece::WPawn), PieceType::Pawn);
    EXPECT_EQ(pieceTypeOf(Piece::WKnight), PieceType::Knight);
    EXPECT_EQ(pieceTypeOf(Piece::WBishop), PieceType::Bishop);
    EXPECT_EQ(pieceTypeOf(Piece::WRook), PieceType::Rook);
    EXPECT_EQ(pieceTypeOf(Piece::WQueen), PieceType::Queen);
    EXPECT_EQ(pieceTypeOf(Piece::WKing), PieceType::King);
    EXPECT_EQ(pieceTypeOf(Piece::BPawn), PieceType::Pawn);
    EXPECT_EQ(pieceTypeOf(Piece::BKnight), PieceType::Knight);
    EXPECT_EQ(pieceTypeOf(Piece::BBishop), PieceType::Bishop);
    EXPECT_EQ(pieceTypeOf(Piece::BRook), PieceType::Rook);
    EXPECT_EQ(pieceTypeOf(Piece::BQueen), PieceType::Queen);
    EXPECT_EQ(pieceTypeOf(Piece::BKing), PieceType::King);
}

TEST(TypesTest, PieceColorOf) {
    EXPECT_EQ(pieceColorOf(Piece::WPawn), WHITE);
    EXPECT_EQ(pieceColorOf(Piece::WKnight), WHITE);
    EXPECT_EQ(pieceColorOf(Piece::WBishop), WHITE);
    EXPECT_EQ(pieceColorOf(Piece::WRook), WHITE);
    EXPECT_EQ(pieceColorOf(Piece::WQueen), WHITE);
    EXPECT_EQ(pieceColorOf(Piece::WKing), WHITE);
    EXPECT_EQ(pieceColorOf(Piece::BPawn), BLACK);
    EXPECT_EQ(pieceColorOf(Piece::BKnight), BLACK);
    EXPECT_EQ(pieceColorOf(Piece::BBishop), BLACK);
    EXPECT_EQ(pieceColorOf(Piece::BRook), BLACK);
    EXPECT_EQ(pieceColorOf(Piece::BQueen), BLACK);
    EXPECT_EQ(pieceColorOf(Piece::BKing), BLACK);
}
