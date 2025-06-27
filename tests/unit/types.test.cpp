#include "types.hpp"

#include <gtest/gtest.h>

TEST(Types_sqFromCoords, CorrectValues) {
    EXPECT_EQ(makeSquare(File::F1, Rank::R1), A1);
    EXPECT_EQ(makeSquare(File::F1, Rank::R8), A8);
    EXPECT_EQ(makeSquare(File::F8, Rank::R1), H1);
    EXPECT_EQ(makeSquare(File::F8, Rank::R8), H8);
    EXPECT_EQ(makeSquare(File::F4, Rank::R4), D4);
}

TEST(Types_sqFromString, CorrectValues) {
    EXPECT_EQ(makeSquare("a1"), A1);
    EXPECT_EQ(makeSquare("a8"), A8);
    EXPECT_EQ(makeSquare("h1"), H1);
    EXPECT_EQ(makeSquare("h8"), H8);
    EXPECT_EQ(makeSquare("d4"), D4);
}

TEST(Types_rankFromSq, CorrectValues) {
    EXPECT_EQ(rankOf(A1), Rank::R1);
    EXPECT_EQ(rankOf(H1), Rank::R1);
    EXPECT_EQ(rankOf(A4), Rank::R4);
    EXPECT_EQ(rankOf(H4), Rank::R4);
    EXPECT_EQ(rankOf(A8), Rank::R8);
    EXPECT_EQ(rankOf(H8), Rank::R8);
}

TEST(Types_fileFromSq, CorrectValues) {
    EXPECT_EQ(fileOf(A1), File::F1);
    EXPECT_EQ(fileOf(A8), File::F1);
    EXPECT_EQ(fileOf(D1), File::F4);
    EXPECT_EQ(fileOf(D8), File::F4);
    EXPECT_EQ(fileOf(H1), File::F8);
    EXPECT_EQ(fileOf(H8), File::F8);
}

TEST(Types_makePiece, CorrectValues) {
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

TEST(Types_getPieceType, CorrectValues) {
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

TEST(Types_getPieceColor, CorrectValues) {
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

TEST(Types_pawnMovePush, CorrectValues) {
    Square sq = pawnMove<WHITE, PawnMove::Push, true>(E4);
    EXPECT_EQ(sq, E5);
    sq = pawnMove<WHITE, PawnMove::Push, false>(E5);
    EXPECT_EQ(sq, E4);

    sq = pawnMove<BLACK, PawnMove::Push, true>(E5);
    EXPECT_EQ(sq, E4);
    sq = pawnMove<BLACK, PawnMove::Push, false>(E4);
    EXPECT_EQ(sq, E5);
}

TEST(Types_pawnMoveRight, CorrectValues) {
    Square sq = pawnMove<WHITE, PawnMove::Right, true>(D4);
    EXPECT_EQ(sq, E5);
    sq = pawnMove<WHITE, PawnMove::Right, false>(E5);
    EXPECT_EQ(sq, D4);

    sq = pawnMove<BLACK, PawnMove::Right, true>(E5);
    EXPECT_EQ(sq, D4);
    sq = pawnMove<BLACK, PawnMove::Right, false>(D4);
    EXPECT_EQ(sq, E5);
}

TEST(Types_pawnMoveLeft, CorrectValues) {
    Square sq;
    sq = pawnMove<WHITE, PawnMove::Left, true>(E4);
    EXPECT_EQ(sq, D5);
    sq = pawnMove<WHITE, PawnMove::Left, false>(D5);
    EXPECT_EQ(sq, E4);

    sq = pawnMove<BLACK, PawnMove::Left, true>(D5);
    EXPECT_EQ(sq, E4);
    sq = pawnMove<BLACK, PawnMove::Left, false>(E4);
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
    EXPECT_EQ(File::F1 + 1, File::F2);
    EXPECT_EQ(File::F1, File::F2 - 1);
}

TEST(Types_Rank, Arithmetic) {
    EXPECT_EQ(Rank::R1 + 1, Rank::R2);
    EXPECT_EQ(Rank::R1, Rank::R2 - 1);
}
