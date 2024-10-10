#include "chess.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

class ChessTest : public ::testing::Test {
   protected:
    void SetUp() override { Magics::init(); }
};

TEST_F(ChessTest, ChessToFEN) {
    for (auto fen : G::FENS) {
        Chess c = Chess(fen);
        EXPECT_EQ(c.toFEN(), fen);
    }
}

TEST_F(ChessTest, Make) {
    Chess c = Chess(G::STARTFEN);
    c.make(Move(G1, F3));

    EXPECT_EQ(c.toFEN(),
              "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1");
}

TEST_F(ChessTest, MakeCapt) {
    Chess c = Chess(G::POS2);
    c.make(Move(E2, A6));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/Bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1");
}

TEST_F(ChessTest, MakeCaptRook) {
    Chess c = Chess("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1 1");
    c.make(Move(A1, A8));
    EXPECT_EQ(c.toFEN(), "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1");
}

auto ENPASSANT_FEN = "4k3/8/8/8/1p6/8/P7/4K3 w - - 0 1",
     ENPASSANT_SQ = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1",
     ENPASSANT_CAPT = "4k3/8/8/8/8/p7/8/4K3 w - - 0 2";

TEST_F(ChessTest, MakeEnpassantSq) {
    Chess c = Chess(ENPASSANT_FEN);
    c.make(Move(A2, A4));
    EXPECT_EQ(c.toFEN(), ENPASSANT_SQ);
}

TEST_F(ChessTest, MakeEnpassantCapt) {
    Chess c = Chess(ENPASSANT_SQ);
    c.make(Move(B4, A3, ENPASSANT));
    EXPECT_EQ(c.toFEN(), ENPASSANT_CAPT);
}

TEST_F(ChessTest, MakeCastleOO) {
    Chess c = Chess(G::POS2);
    c.make(Move(E1, G1, CASTLE));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R4RK1 b kq - 1 1");
}

TEST_F(ChessTest, MakeCastleOOO) {
    Chess c = Chess(G::POS2);
    c.make(Move(E1, C1, CASTLE));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2KR3R b kq - 1 1");
}

TEST_F(ChessTest, MakeKingMoveDisableCastleRights) {
    Chess c = Chess(G::POS2);
    c.make(Move(E1, D1));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R2K3R b kq - 1 1");
}

TEST_F(ChessTest, MakeRookMoveDisableCastleOORights) {
    Chess c = Chess(G::POS2);
    c.make(Move(H1, F1));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3KR2 b Qkq - 1 1");
}

TEST_F(ChessTest, MakeRookMoveDisableCastleOOORights) {
    Chess c = Chess(G::POS2);
    c.make(Move(A1, C1));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 1 1");
}

TEST_F(ChessTest, MakePromotion) {
    Chess c = Chess("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    c.make(Move(A7, A8, PROMOTION, QUEEN));
    EXPECT_EQ(c.toFEN(), "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1");
}

TEST_F(ChessTest, MakeUnderPromotion) {
    Chess c = Chess("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    c.make(Move(A7, A8, PROMOTION, BISHOP));
    EXPECT_EQ(c.toFEN(), "B3k3/8/8/8/8/8/8/4K3 b - - 0 1");
}
