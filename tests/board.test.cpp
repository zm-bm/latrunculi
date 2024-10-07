#include "board.hpp"

#include <gtest/gtest.h>

#include "globals.hpp"

class BoardTest : public ::testing::Test {
   protected:
    void SetUp() override {
        Magics::init();
        emptyBoard = new Board(G::EMPTYFEN);
        startBoard = new Board(G::STARTFEN);
    }

    void TearDown() override {
        delete emptyBoard;
        delete startBoard;
    }

    Board* emptyBoard;
    Board* startBoard;
};

TEST_F(BoardTest, GetPieces) {
    EXPECT_EQ(emptyBoard->getPieces<KING>(WHITE), BB::set(E1));
    EXPECT_EQ(startBoard->getPieces<PAWN>(WHITE), G::RANK_MASK[RANK2]);
}

TEST_F(BoardTest, Count) {
    EXPECT_EQ(emptyBoard->count<KING>(WHITE), 1);
    EXPECT_EQ(emptyBoard->count<KING>(), 2);
    EXPECT_EQ(startBoard->count<PAWN>(WHITE), 8);
    EXPECT_EQ(startBoard->count<PAWN>(), 16);
}

TEST_F(BoardTest, Occupancy) {
    EXPECT_EQ(emptyBoard->count<KING>(WHITE), 1);
    EXPECT_EQ(startBoard->occupancy(),
              G::RANK_MASK[RANK1] | G::RANK_MASK[RANK2] | G::RANK_MASK[RANK7] |
                  G::RANK_MASK[RANK8]);
}

TEST_F(BoardTest, GetPiece) {
    EXPECT_EQ(emptyBoard->getPiece(E1), Types::makePiece(WHITE, KING));
    EXPECT_EQ(emptyBoard->getPiece(E2), NO_PIECE);
}

TEST_F(BoardTest, AddPiece) {
    emptyBoard->addPiece(E2, WHITE, PAWN);
    EXPECT_EQ(emptyBoard->getPieces<PAWN>(WHITE), BB::set(E2));
    EXPECT_EQ(emptyBoard->count<PAWN>(WHITE), 1);
    EXPECT_EQ(emptyBoard->occupancy(), BB::set(E8) | BB::set(E2) | BB::set(E1));
    EXPECT_EQ(emptyBoard->getPiece(E2), Types::makePiece(WHITE, PAWN));
}

TEST_F(BoardTest, RemovePiece) {
    emptyBoard->addPiece(E2, WHITE, PAWN);
    emptyBoard->removePiece(E2, WHITE, PAWN);
    EXPECT_EQ(emptyBoard->getPieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(emptyBoard->count<PAWN>(WHITE), 0);
    EXPECT_EQ(emptyBoard->occupancy(), BB::set(E8) | BB::set(E1));
}

TEST_F(BoardTest, MovePiece) {
    emptyBoard->movePiece(E1, D1, WHITE, KING);
    EXPECT_EQ(emptyBoard->getPieces<KING>(WHITE), BB::set(D1));
    EXPECT_EQ(emptyBoard->count<KING>(WHITE), 1);
    EXPECT_EQ(emptyBoard->occupancy(), BB::set(E8) | BB::set(D1));
    EXPECT_EQ(emptyBoard->getPiece(E1), NO_PIECE);
    EXPECT_EQ(emptyBoard->getPiece(D1), Types::makePiece(WHITE, KING));
}

TEST_F(BoardTest, DiagonalSliders) {
    U64 diagonalSliders = startBoard->diagonalSliders(WHITE);
    EXPECT_EQ(diagonalSliders, BB::set(C1) | BB::set(D1) | BB::set(F1));
}

TEST_F(BoardTest, StraightSliders) {
    U64 straightSliders = startBoard->straightSliders(WHITE);
    EXPECT_EQ(straightSliders, BB::set(A1) | BB::set(D1) | BB::set(H1));
}

TEST_F(BoardTest, AttacksTo) {
    U64 attackers = startBoard->attacksTo(A3, WHITE);
    EXPECT_EQ(attackers, BB::set(B2) | BB::set(B1));
}

// TEST_F(BoardTest, BoardToFEN) {
//   EXPECT_EQ(startBoard->toFEN(), G::STARTFEN);

//   auto fen =
//     "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 0";
//   board = Board(fen);
//   EXPECT_EQ(board.toFEN(), fen);
// }
