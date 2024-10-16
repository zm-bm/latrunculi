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
    EXPECT_EQ(emptyBoard->getPieces<KING>(WHITE), BB::set(E1))
        << "should get piece bitboard of white king";
    EXPECT_EQ(startBoard->getPieces<PAWN>(WHITE), G::RANK_MASK[RANK2])
        << "should get bitboard of white pawns from start board";
}

TEST_F(BoardTest, Count) {
    EXPECT_EQ(emptyBoard->count<KING>(WHITE), 1) << "should count 1 white king";
    EXPECT_EQ(startBoard->count<PAWN>(WHITE), 8)
        << "should count 8 white pawns on start board";
    EXPECT_EQ(emptyBoard->count<KING>(), 2) << "should count 2 kings";
    EXPECT_EQ(startBoard->count<PAWN>(), 16)
        << "should count 16 pawns on start board";
}

TEST_F(BoardTest, GetPieceCount) {
    EXPECT_EQ(emptyBoard->getPieceCount(WHITE), 0)
        << "should count 0 white pieces on empty board";
    EXPECT_EQ(emptyBoard->getPieceCount(BLACK), 0)
        << "should count 0 black pieces on empty board";
    EXPECT_EQ(startBoard->getPieceCount(WHITE), 7)
        << "should count 7 white pieces on start board";
    EXPECT_EQ(startBoard->getPieceCount(BLACK), 7)
        << "should count 7 black pieces on start board";
}

TEST_F(BoardTest, Occupancy) {
    U64 expected = (G::RANK_MASK[RANK1] | G::RANK_MASK[RANK2] |
                    G::RANK_MASK[RANK7] | G::RANK_MASK[RANK8]);
    EXPECT_EQ(startBoard->occupancy(), expected)
        << "should get occupancy of starting board";
}

TEST_F(BoardTest, GetPiece) {
    EXPECT_EQ(emptyBoard->getPiece(E1), Types::makePiece(WHITE, KING))
        << "should have a white king on e1";
    EXPECT_EQ(emptyBoard->getPiece(E2), NO_PIECE) << "should have an empty e2";
    EXPECT_EQ(startBoard->getPiece(A2), Types::makePiece(WHITE, PAWN))
        << "should have a white pawn on a1";
    EXPECT_EQ(startBoard->getPiece(A3), NO_PIECE) << "should have an empty a3";
}

TEST_F(BoardTest, GetPieceRole) {
    EXPECT_EQ(emptyBoard->getPieceRole(E1), KING) << "should have a king on e1";
    EXPECT_EQ(emptyBoard->getPieceRole(E2), NO_PIECE_ROLE)
        << "should have an empty e2";
    EXPECT_EQ(startBoard->getPieceRole(A2), PAWN) << "should have a pawn on a2";
    EXPECT_EQ(startBoard->getPieceRole(A3), NO_PIECE_ROLE)
        << "should have an empty a3";
}

TEST_F(BoardTest, GetKingSq) {
    EXPECT_EQ(startBoard->getKingSq(WHITE), E1)
        << "should have white king on e1";
    EXPECT_EQ(startBoard->getKingSq(BLACK), E8)
        << "should have black king on e8";
}

TEST_F(BoardTest, AddPiece) {
    emptyBoard->addPiece(E2, WHITE, PAWN);
    EXPECT_EQ(emptyBoard->getPiece(E2), Types::makePiece(WHITE, PAWN))
        << "should add a white pawn on e2";
    EXPECT_EQ(emptyBoard->getPieces<PAWN>(WHITE), BB::set(E2))
        << "should add a white pawn on e2";
    EXPECT_EQ(emptyBoard->count<PAWN>(WHITE), 1) << "should add 1 white pawn";
    EXPECT_EQ(emptyBoard->occupancy(), BB::set(E8) | BB::set(E2) | BB::set(E1))
        << "should add to occupancy";
}

TEST_F(BoardTest, RemovePiece) {
    emptyBoard->addPiece(E2, WHITE, PAWN);
    emptyBoard->removePiece(E2, WHITE, PAWN);
    EXPECT_EQ(emptyBoard->getPieces<PAWN>(WHITE), 0x0)
        << "should remove white pawn from bitboard";
    EXPECT_EQ(emptyBoard->count<PAWN>(WHITE), 0)
        << "should have zero white pawns";
    EXPECT_EQ(emptyBoard->occupancy(), BB::set(E8) | BB::set(E1))
        << "should remove from occupancy";
}

TEST_F(BoardTest, MovePiece) {
    emptyBoard->movePiece(E1, D1, WHITE, KING);
    EXPECT_EQ(emptyBoard->getPieces<KING>(WHITE), BB::set(D1))
        << "should move white king to d1";
    EXPECT_EQ(emptyBoard->count<KING>(WHITE), 1) << "should have 1 white king";
    EXPECT_EQ(emptyBoard->occupancy(), BB::set(E8) | BB::set(D1))
        << "should have occupancy on d1";
    EXPECT_EQ(emptyBoard->getPiece(E1), NO_PIECE)
        << "should have empty e1 square";
    EXPECT_EQ(emptyBoard->getPiece(D1), Types::makePiece(WHITE, KING))
        << "should have white pawn on d1";
}

TEST_F(BoardTest, DiagonalSliders) {
    U64 diagonalSliders = startBoard->diagonalSliders(WHITE);
    EXPECT_EQ(diagonalSliders, BB::set(C1) | BB::set(D1) | BB::set(F1))
        << "should have diag sliders on c1, d1, f1";
}

TEST_F(BoardTest, StraightSliders) {
    U64 straightSliders = startBoard->straightSliders(WHITE);
    EXPECT_EQ(straightSliders, BB::set(A1) | BB::set(D1) | BB::set(H1))
        << "should have straight sliders on a1, d1, h1";
}

TEST_F(BoardTest, AttacksTo) {
    U64 attackers = startBoard->attacksTo(A3, WHITE);
    EXPECT_EQ(attackers, BB::set(B2) | BB::set(B1))
        << "should attack a3 from b2 and b1";
}

TEST_F(BoardTest, CalculateCheckBlockers) {
    EXPECT_EQ(startBoard->calculateCheckBlockers(BLACK, BLACK), 0)
        << "start board should have no pins";
    EXPECT_EQ(startBoard->calculateCheckBlockers(WHITE, WHITE), 0)
        << "start board should have no pins";
    EXPECT_EQ(pinBoard->calculateCheckBlockers(WHITE, WHITE), BB::set(B5))
        << "should have a pin on b5";
    EXPECT_EQ(pinBoard->calculateCheckBlockers(BLACK, BLACK), BB::set(F4))
        << "should have a pin on f4";
}

TEST_F(BoardTest, CalculateDiscoveredChecks) {
    Board b = Board("8/2p5/3p4/Kp5r/1R3P1k/8/4P1P1/8 w - -");
    EXPECT_EQ(b.calculateDiscoveredCheckers(WHITE), BB::set(F4))
        << "should have a discovered check on f4";
    EXPECT_EQ(b.calculateDiscoveredCheckers(BLACK), BB::set(B5))
        << "should have a discovered check on b5";
}

TEST_F(BoardTest, CalculatePinnedPieces) {
    EXPECT_EQ(startBoard->calculatePinnedPieces(WHITE), 0)
        << "start board should have no pins";
    EXPECT_EQ(startBoard->calculatePinnedPieces(BLACK), 0)
        << "start board should have no pins";
    EXPECT_EQ(pinBoard->calculatePinnedPieces(WHITE), BB::set(B5))
        << "should have a pin on B5";
    EXPECT_EQ(pinBoard->calculatePinnedPieces(BLACK), BB::set(F4))
        << "should have a pin on F4";
}

TEST_F(BoardTest, CalculateCheckingPiecesW) {
    Board b = Board(G::POS4W);
    EXPECT_EQ(b.calculateCheckingPieces(WHITE), BB::set(B6))
        << "should have a white checker on b6";
    EXPECT_EQ(b.calculateCheckingPieces(BLACK), 0)
        << "should have no checking piece";
}

TEST_F(BoardTest, CalculateCheckingPiecesB) {
    Board b = Board(G::POS4B);
    EXPECT_EQ(b.calculateCheckingPieces(WHITE), 0)
        << "should have no checking piece";
    EXPECT_EQ(b.calculateCheckingPieces(BLACK), BB::set(B3))
        << "should have a black checker on b3";
}

TEST_F(BoardTest, IsBitboardAttacked) {
    EXPECT_FALSE(pinBoard->isBitboardAttacked(G::FILE_MASK[FILE7], WHITE))
        << "white should attack G file";
    EXPECT_TRUE(pinBoard->isBitboardAttacked(G::FILE_MASK[FILE8], WHITE))
        << "white should not attack H file";
    EXPECT_FALSE(pinBoard->isBitboardAttacked(G::FILE_MASK[FILE1], BLACK))
        << "black should not attack A file";
    EXPECT_TRUE(pinBoard->isBitboardAttacked(G::FILE_MASK[FILE2], BLACK))
        << "black should attack B file";
}
