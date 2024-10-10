#include "board.hpp"

#include <gtest/gtest.h>

#include "globals.hpp"

class BoardTest : public ::testing::Test {
   protected:
    void SetUp() override {
        Magics::init();
        emptyBoard = new Board("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        startBoard = new Board(G::STARTFEN);
        pinBoard = new Board(G::POS3);
    }

    void TearDown() override {
        delete emptyBoard;
        delete startBoard;
        delete pinBoard;
    }

    Board *emptyBoard, *startBoard, *pinBoard;
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

TEST_F(BoardTest, GetPieceCount) {
    EXPECT_EQ(emptyBoard->getPieceCount(WHITE), 0);
    EXPECT_EQ(emptyBoard->getPieceCount(BLACK), 0);
    EXPECT_EQ(startBoard->getPieceCount(WHITE), 7);
    EXPECT_EQ(startBoard->getPieceCount(BLACK), 7);
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
    EXPECT_EQ(startBoard->getPiece(A2), Types::makePiece(WHITE, PAWN));
    EXPECT_EQ(startBoard->getPiece(A3), NO_PIECE);
}

TEST_F(BoardTest, GetPieceRole) {
    EXPECT_EQ(emptyBoard->getPieceRole(E1), KING);
    EXPECT_EQ(emptyBoard->getPieceRole(E2), NO_PIECE_ROLE);
    EXPECT_EQ(startBoard->getPieceRole(A2), PAWN);
    EXPECT_EQ(startBoard->getPieceRole(A3), NO_PIECE_ROLE);
}

TEST_F(BoardTest, GetKingSq) {
    EXPECT_EQ(startBoard->getKingSq(WHITE), E1);
    EXPECT_EQ(startBoard->getKingSq(BLACK), E8);
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

TEST_F(BoardTest, CalculateCheckBlockers) {
    EXPECT_EQ(startBoard->calculateCheckBlockers(BLACK, BLACK), 0);
    EXPECT_EQ(startBoard->calculateCheckBlockers(WHITE, WHITE), 0);
    EXPECT_EQ(pinBoard->calculateCheckBlockers(WHITE, WHITE), BB::set(B5));
    EXPECT_EQ(pinBoard->calculateCheckBlockers(BLACK, BLACK), BB::set(F4));
}

TEST_F(BoardTest, CalculateDiscoveredChecks) {
    Board b = Board("8/2p5/3p4/Kp5r/1R3P1k/8/4P1P1/8 w - -");
    EXPECT_EQ(b.calculateDiscoveredCheckers(WHITE), BB::set(F4));
    EXPECT_EQ(b.calculateDiscoveredCheckers(BLACK), BB::set(B5));
}

TEST_F(BoardTest, CalculatePinnedPieces) {
    EXPECT_EQ(startBoard->calculatePinnedPieces(WHITE), 0);
    EXPECT_EQ(startBoard->calculatePinnedPieces(BLACK), 0);
    EXPECT_EQ(pinBoard->calculatePinnedPieces(WHITE), BB::set(B5));
    EXPECT_EQ(pinBoard->calculatePinnedPieces(BLACK), BB::set(F4));
}

TEST_F(BoardTest, CalculateCheckingPieces) {
    Board b = Board(G::POS4W);
    EXPECT_EQ(b.calculateCheckingPieces(WHITE), BB::set(B6));
    EXPECT_EQ(b.calculateCheckingPieces(BLACK), 0);
    b = Board(G::POS4B);
    EXPECT_EQ(b.calculateCheckingPieces(WHITE), 0);
    EXPECT_EQ(b.calculateCheckingPieces(BLACK), BB::set(B3));
}

TEST_F(BoardTest, IsBitboardAttacked) {
    EXPECT_FALSE(pinBoard->isBitboardAttacked(G::FILE_MASK[FILE7], WHITE));
    EXPECT_TRUE(pinBoard->isBitboardAttacked(G::FILE_MASK[FILE8], WHITE));
    EXPECT_FALSE(pinBoard->isBitboardAttacked(G::FILE_MASK[FILE1], BLACK));
    EXPECT_TRUE(pinBoard->isBitboardAttacked(G::FILE_MASK[FILE2], BLACK));
}
