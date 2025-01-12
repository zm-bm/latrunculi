#include "board.hpp"

#include <gtest/gtest.h>

#include "constants.hpp"

TEST(Board_addPiece, AddE2WhitePawn) {
    Board emptyBoard(EMPTYFEN);
    emptyBoard.addPiece(E2, WHITE, PAWN);

    EXPECT_EQ(emptyBoard.getPiece(E2), Defs::makePiece(WHITE, PAWN));
    EXPECT_EQ(emptyBoard.getPieces<PAWN>(WHITE), BB::set(E2));
    EXPECT_EQ(emptyBoard.count<PAWN>(WHITE), 1);
    EXPECT_EQ(emptyBoard.occupancy(), BB::set(E8) | BB::set(E2) | BB::set(E1));
}

TEST(Board_removePiece, RemoveE2WhitePawn) {
    Board emptyBoard(E2PAWN);
    emptyBoard.removePiece(E2, WHITE, PAWN);

    EXPECT_EQ(emptyBoard.getPieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(emptyBoard.count<PAWN>(WHITE), 0);
    EXPECT_EQ(emptyBoard.occupancy(), BB::set(E8) | BB::set(E1));
}

TEST(Board_movePiece, MoveWhiteKing) {
    Board emptyBoard(EMPTYFEN);
    emptyBoard.movePiece(E1, D1, WHITE, KING);

    EXPECT_EQ(emptyBoard.getPieces<KING>(WHITE), BB::set(D1));
    EXPECT_EQ(emptyBoard.count<KING>(WHITE), 1);
    EXPECT_EQ(emptyBoard.occupancy(), BB::set(E8) | BB::set(D1));
    EXPECT_EQ(emptyBoard.getPiece(E1), NO_PIECE);
    EXPECT_EQ(emptyBoard.getPiece(D1), Defs::makePiece(WHITE, KING));
}

TEST(Board_getPieces, EmptyPosition) {
    Board b(EMPTYFEN);

    EXPECT_EQ(b.getPieces<KING>(WHITE), BB::set(E1));
    EXPECT_EQ(b.getPieces<KING>(BLACK), BB::set(E8));
    EXPECT_EQ(b.getPieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(b.getPieces<PAWN>(BLACK), 0x0);
    EXPECT_EQ(b.getPieces<KNIGHT>(WHITE), 0x0);
    EXPECT_EQ(b.getPieces<KNIGHT>(BLACK), 0x0);
    EXPECT_EQ(b.getPieces<BISHOP>(WHITE), 0x0);
    EXPECT_EQ(b.getPieces<BISHOP>(BLACK), 0x0);
    EXPECT_EQ(b.getPieces<ROOK>(WHITE), 0x0);
    EXPECT_EQ(b.getPieces<ROOK>(BLACK), 0x0);
    EXPECT_EQ(b.getPieces<QUEEN>(WHITE), 0x0);
    EXPECT_EQ(b.getPieces<QUEEN>(BLACK), 0x0);
}

TEST(Board_getPieces, StartPosition) {
    Board b(STARTFEN);

    EXPECT_EQ(b.getPieces<KING>(WHITE), BB::set(E1));
    EXPECT_EQ(b.getPieces<KING>(BLACK), BB::set(E8));
    EXPECT_EQ(b.getPieces<PAWN>(WHITE), BB::RANK_MASK[RANK2]);
    EXPECT_EQ(b.getPieces<PAWN>(BLACK), BB::RANK_MASK[RANK7]);
    EXPECT_EQ(b.getPieces<KNIGHT>(WHITE), BB::set(B1) | BB::set(G1));
    EXPECT_EQ(b.getPieces<KNIGHT>(BLACK), BB::set(B8) | BB::set(G8));
    EXPECT_EQ(b.getPieces<BISHOP>(WHITE), BB::set(C1) | BB::set(F1));
    EXPECT_EQ(b.getPieces<BISHOP>(BLACK), BB::set(C8) | BB::set(F8));
    EXPECT_EQ(b.getPieces<ROOK>(WHITE), BB::set(A1) | BB::set(H1));
    EXPECT_EQ(b.getPieces<ROOK>(BLACK), BB::set(A8) | BB::set(H8));
    EXPECT_EQ(b.getPieces<QUEEN>(WHITE), BB::set(D1));
    EXPECT_EQ(b.getPieces<QUEEN>(BLACK), BB::set(D8));
}

TEST(Board_getPiece, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.getPiece(E1), Defs::makePiece(WHITE, KING));
    EXPECT_EQ(b.getPiece(E2), NO_PIECE);
}

TEST(Board_getPiece, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.getPiece(A2), Defs::makePiece(WHITE, PAWN));
    EXPECT_EQ(b.getPiece(A3), NO_PIECE);
}

TEST(Board_getPieceType, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.getPieceType(E1), KING);
    EXPECT_EQ(b.getPieceType(E2), NO_PIECE_TYPE);
}

TEST(Board_getPieceType, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.getPieceType(A2), PAWN);
    EXPECT_EQ(b.getPieceType(A3), NO_PIECE_TYPE);
}

TEST(Board_getKingSq, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.getKingSq(WHITE), E1);
    EXPECT_EQ(b.getKingSq(BLACK), E8);
}

TEST(Board_getKingSq, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.getKingSq(WHITE), E1);
    EXPECT_EQ(b.getKingSq(BLACK), E8);
}

TEST(Board_occupancy, EmptyPosition) {
    U64 expected = BB::set(E1) | BB::set(E8);
    EXPECT_EQ(Board(EMPTYFEN).occupancy(), expected);
}

TEST(Board_occupancy, StartPosition) {
    U64 expected =
        (BB::RANK_MASK[RANK1] | BB::RANK_MASK[RANK2] | BB::RANK_MASK[RANK7] | BB::RANK_MASK[RANK8]);
    EXPECT_EQ(Board(STARTFEN).occupancy(), expected);
}

TEST(Board_diagonalSliders, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.diagonalSliders(WHITE), 0);
    EXPECT_EQ(b.diagonalSliders(BLACK), 0);
}

TEST(Board_diagonalSliders, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.diagonalSliders(WHITE), BB::set(C1) | BB::set(D1) | BB::set(F1));
    EXPECT_EQ(b.diagonalSliders(BLACK), BB::set(C8) | BB::set(D8) | BB::set(F8));
}

TEST(Board_straightSliders, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.straightSliders(WHITE), 0);
    EXPECT_EQ(b.straightSliders(BLACK), 0);
}

TEST(Board_straightSliders, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.straightSliders(WHITE), BB::set(A1) | BB::set(D1) | BB::set(H1));
    EXPECT_EQ(b.straightSliders(BLACK), BB::set(A8) | BB::set(D8) | BB::set(H8));
}

TEST(Board_attacksTo, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.attacksTo(A2, WHITE), 0);
    EXPECT_EQ(b.attacksTo(A3, WHITE), 0);
    EXPECT_EQ(b.attacksTo(A4, WHITE), 0);
    EXPECT_EQ(b.attacksTo(B2, WHITE), 0);
    EXPECT_EQ(b.attacksTo(B3, WHITE), 0);
    EXPECT_EQ(b.attacksTo(B4, WHITE), 0);
}

TEST(Board_attacksTo, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.attacksTo(A2, WHITE), BB::set(A1));
    EXPECT_EQ(b.attacksTo(A3, WHITE), BB::set(B2) | BB::set(B1));
    EXPECT_EQ(b.attacksTo(A4, WHITE), 0);
    EXPECT_EQ(b.attacksTo(B2, WHITE), BB::set(C1));
    EXPECT_EQ(b.attacksTo(B3, WHITE), BB::set(A2) | BB::set(C2));
    EXPECT_EQ(b.attacksTo(B4, WHITE), 0);
}

TEST(Board_calculateCheckBlockers, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.calculateCheckBlockers(BLACK, BLACK), 0);
    EXPECT_EQ(b.calculateCheckBlockers(WHITE, WHITE), 0);
}

TEST(Board_calculateCheckBlockers, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.calculateCheckBlockers(BLACK, BLACK), 0);
    EXPECT_EQ(b.calculateCheckBlockers(WHITE, WHITE), 0);
}

TEST(Board_calculateCheckBlockers, PinnedPosition) {
    Board b(POS3);
    EXPECT_EQ(b.calculateCheckBlockers(WHITE, WHITE), BB::set(B5));
    EXPECT_EQ(b.calculateCheckBlockers(BLACK, BLACK), BB::set(F4));
}

TEST(Board_calculateDiscoveredCheckers, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.calculateDiscoveredCheckers(WHITE), 0);
    EXPECT_EQ(b.calculateDiscoveredCheckers(BLACK), 0);
}

TEST(Board_calculateDiscoveredCheckers, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.calculateDiscoveredCheckers(WHITE), 0);
    EXPECT_EQ(b.calculateDiscoveredCheckers(BLACK), 0);
}

TEST(Board_calculateDiscoveredCheckers, DiscoveredChecksPosition) {
    Board b("8/2p5/3p4/Kp5r/1R3P1k/8/4P1P1/8 w - -");
    EXPECT_EQ(b.calculateDiscoveredCheckers(WHITE), BB::set(F4));
    EXPECT_EQ(b.calculateDiscoveredCheckers(BLACK), BB::set(B5));
}

TEST(Board_calculatePinnedPieces, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.calculatePinnedPieces(WHITE), 0);
    EXPECT_EQ(b.calculatePinnedPieces(BLACK), 0);
}

TEST(Board_calculatePinnedPieces, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.calculatePinnedPieces(WHITE), 0);
    EXPECT_EQ(b.calculatePinnedPieces(BLACK), 0);
}

TEST(Board_calculatePinnedPieces, PinnedPosition) {
    Board b(POS3);
    EXPECT_EQ(b.calculatePinnedPieces(WHITE), BB::set(B5));
    EXPECT_EQ(b.calculatePinnedPieces(BLACK), BB::set(F4));
}

TEST(Board_calculateCheckingPieces, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.calculateCheckingPieces(WHITE), 0);
    EXPECT_EQ(b.calculateCheckingPieces(BLACK), 0);
}

TEST(Board_calculateCheckingPieces, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.calculateCheckingPieces(WHITE), 0);
    EXPECT_EQ(b.calculateCheckingPieces(BLACK), 0);
}

TEST(Board_calculateCheckingPieces, WhiteCheckPosition) {
    Board b(POS4W);
    EXPECT_EQ(b.calculateCheckingPieces(WHITE), BB::set(B6));
    EXPECT_EQ(b.calculateCheckingPieces(BLACK), 0);
}

TEST(Board_calculateCheckingPieces, BlackCheckPosition) {
    Board b(POS4B);
    EXPECT_EQ(b.calculateCheckingPieces(WHITE), 0);
    EXPECT_EQ(b.calculateCheckingPieces(BLACK), BB::set(B3));
}

TEST(Board_isBitboardAttacked, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_TRUE(b.isBitboardAttacked(BB::RANK_MASK[RANK1], WHITE));
    EXPECT_FALSE(b.isBitboardAttacked(BB::RANK_MASK[RANK3], WHITE));
    EXPECT_TRUE(b.isBitboardAttacked(BB::RANK_MASK[FILE8], BLACK));
    EXPECT_FALSE(b.isBitboardAttacked(BB::RANK_MASK[RANK6], BLACK));
}

TEST(Board_isBitboardAttacked, StartPosition) {
    Board b(STARTFEN);
    EXPECT_TRUE(b.isBitboardAttacked(BB::RANK_MASK[RANK1], WHITE));
    EXPECT_TRUE(b.isBitboardAttacked(BB::RANK_MASK[RANK3], WHITE));
    EXPECT_FALSE(b.isBitboardAttacked(BB::RANK_MASK[RANK4], WHITE));
    EXPECT_TRUE(b.isBitboardAttacked(BB::RANK_MASK[RANK8], BLACK));
    EXPECT_TRUE(b.isBitboardAttacked(BB::RANK_MASK[RANK6], BLACK));
    EXPECT_FALSE(b.isBitboardAttacked(BB::RANK_MASK[RANK5], BLACK));
}

TEST(Board_isBitboardAttacked, PinnedPosition) {
    Board b(POS3);
    EXPECT_TRUE(b.isBitboardAttacked(BB::FILE_MASK[FILE8], WHITE));
    EXPECT_FALSE(b.isBitboardAttacked(BB::FILE_MASK[FILE7], WHITE));
    EXPECT_TRUE(b.isBitboardAttacked(BB::FILE_MASK[FILE2], BLACK));
    EXPECT_FALSE(b.isBitboardAttacked(BB::FILE_MASK[FILE1], BLACK));
}

TEST(Board_count, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.count<KING>(WHITE), 1);
    EXPECT_EQ(b.count<KING>(BLACK), 1);
    EXPECT_EQ(b.count<PAWN>(WHITE), 0);
    EXPECT_EQ(b.count<PAWN>(BLACK), 0);
    EXPECT_EQ(b.count<KNIGHT>(WHITE), 0);
    EXPECT_EQ(b.count<KNIGHT>(BLACK), 0);
    EXPECT_EQ(b.count<BISHOP>(WHITE), 0);
    EXPECT_EQ(b.count<BISHOP>(BLACK), 0);
    EXPECT_EQ(b.count<ROOK>(WHITE), 0);
    EXPECT_EQ(b.count<ROOK>(BLACK), 0);
    EXPECT_EQ(b.count<QUEEN>(WHITE), 0);
    EXPECT_EQ(b.count<QUEEN>(BLACK), 0);

}

TEST(Board_count, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.count<KING>(WHITE), 1);
    EXPECT_EQ(b.count<KING>(BLACK), 1);
    EXPECT_EQ(b.count<PAWN>(WHITE), 8);
    EXPECT_EQ(b.count<PAWN>(BLACK), 8);
    EXPECT_EQ(b.count<KNIGHT>(WHITE), 2);
    EXPECT_EQ(b.count<KNIGHT>(BLACK), 2);
    EXPECT_EQ(b.count<BISHOP>(WHITE), 2);
    EXPECT_EQ(b.count<BISHOP>(BLACK), 2);
    EXPECT_EQ(b.count<ROOK>(WHITE), 2);
    EXPECT_EQ(b.count<ROOK>(BLACK), 2);
    EXPECT_EQ(b.count<QUEEN>(WHITE), 1);
    EXPECT_EQ(b.count<QUEEN>(BLACK), 1);
}

TEST(Board_nonPawnMaterial, EmptyPosition) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.nonPawnMaterial(WHITE), 0);
    EXPECT_EQ(b.nonPawnMaterial(BLACK), 0);
}

TEST(Board_nonPawnMaterial, StartPosition) {
    Board startBoard(STARTFEN);
    int mat = (2 * Eval::mgPieceValue(KNIGHT) + 2 * Eval::mgPieceValue(BISHOP) +
               2 * Eval::mgPieceValue(ROOK) + Eval::mgPieceValue(QUEEN));
    EXPECT_EQ(startBoard.nonPawnMaterial(WHITE), mat);
    EXPECT_EQ(startBoard.nonPawnMaterial(BLACK), mat);
}

TEST(Board_oppositeBishops, CorrectValues) {
    EXPECT_FALSE(Board(EMPTYFEN).oppositeBishops());
    EXPECT_FALSE(Board(STARTFEN).oppositeBishops());
    EXPECT_FALSE(Board("3bk3/8/8/8/8/8/8/2B1K3 w - - 0 1").oppositeBishops());
    EXPECT_TRUE(Board("3bk3/8/8/8/8/8/8/3BK3 w - - 0 1").oppositeBishops());
}

TEST(Board_passedPawns, StartPosition) {
    Board b(STARTFEN);
    EXPECT_EQ(b.passedPawns(WHITE), 0);
    EXPECT_EQ(b.passedPawns(BLACK), 0);
}

TEST(Board_passedPawns, NoPassedPawns) {
    Board b("4k3/p2p4/8/8/8/8/P1P5/4K3 w - - 0 1");
    EXPECT_EQ(b.passedPawns(WHITE), 0);
    EXPECT_EQ(b.passedPawns(BLACK), 0);
}

TEST(Board_passedPawns, HasPassedPawns) {
    Board b("4k3/p3p3/8/8/8/8/P1P5/4K3 w - - 0 1");
    EXPECT_EQ(b.passedPawns(WHITE), BB::set(C2));
    EXPECT_EQ(b.passedPawns(BLACK), BB::set(E7));
}
