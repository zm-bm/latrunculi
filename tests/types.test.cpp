#include "types.hpp"

#include <gtest/gtest.h>

TEST(TypesColor, InvertFlipsColor) { EXPECT_EQ(WHITE, ~BLACK); }

TEST(TypesSquares, MakeSquareFromFileRank) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, makeSquare(fileOf(sq), rankOf(sq)));
    }
}

TEST(TypesSquares, MakeSquareFromString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(sq, makeSquare(toString(sq)));
    }
}

TEST(TypesSquares, ToString) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        EXPECT_EQ(toString(sq), std::string{toChar(fileOf(sq))} + toChar(rankOf(sq)));
    }
}

TEST(TypesSquare, Arithmetic) {
    EXPECT_EQ(A1 + 1, B1);
    EXPECT_EQ(A1, B1 - 1);
    EXPECT_EQ(A1 + 8, A2);
    EXPECT_EQ(A1, A2 - 8);

    EXPECT_EQ(File::F1 + 1, File::F2);
    EXPECT_EQ(File::F1, File::F2 - 1);

    EXPECT_EQ(Rank::R1 + 1, Rank::R2);
    EXPECT_EQ(Rank::R1, Rank::R2 - 1);
}

TEST(TypesSquares, RelativeRank) {
    EXPECT_EQ(relativeRank(A1, WHITE), Rank::R1);
    EXPECT_EQ(relativeRank(A1, BLACK), Rank::R8);
    EXPECT_EQ(relativeRank(H8, WHITE), Rank::R8);
    EXPECT_EQ(relativeRank(H8, BLACK), Rank::R1);
}

TEST(TypesSquares, PawnMove) {
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Push, true>(E4)), E5);
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Push, false>(E5)), E4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Push, true>(E5)), E4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Push, false>(E4)), E5);

    EXPECT_EQ((pawnMove<WHITE, PawnMove::Right, true>(D4)), E5);
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Right, false>(E5)), D4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Right, true>(E5)), D4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Right, false>(D4)), E5);

    EXPECT_EQ((pawnMove<WHITE, PawnMove::Left, true>(E4)), D5);
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Left, false>(D5)), E4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Left, true>(D5)), E4);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Left, false>(E4)), D5);

    EXPECT_EQ((pawnMove<WHITE, PawnMove::Double, true>(E2)), E4);
    EXPECT_EQ((pawnMove<WHITE, PawnMove::Double, false>(E4)), E2);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Double, true>(D7)), D5);
    EXPECT_EQ((pawnMove<BLACK, PawnMove::Double, false>(D5)), D7);
}

TEST(TypesPieces, MakePiece) {
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

TEST(TypesPieces, PieceTypeOf) {
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

TEST(TypesPieces, PieceColorOf) {
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
