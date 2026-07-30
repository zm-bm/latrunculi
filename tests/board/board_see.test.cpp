#include "board/board.hpp"

#include <gtest/gtest.h>

#include "support/board_fixtures.hpp"

TEST(BoardSeeTest, ValuesAnUndefendedCapture) {
    Board b("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -");
    EXPECT_EQ(b.see(Move(E1, E5)), eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, AccountsForTheRecaptureSequence) {
    Board b("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
    EXPECT_EQ(b.see(Move(D3, E5)), eval::piece(PAWN).mg - eval::piece(KNIGHT).mg);
}

TEST(BoardSeeTest, PrefersPawnOverKnightRecapturer) {
    Board b("7k/1B1p4/4p3/P2p4/2P5/2n5/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(B7, D5)), eval::piece(PAWN).mg - eval::piece(BISHOP).mg);
}

TEST(BoardSeeTest, HandlesKingRecapture) {
    Board b("8/8/4k3/3p4/3Q4/8/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(D4, D5)), eval::piece(PAWN).mg - eval::piece(QUEEN).mg);
}

TEST(BoardSeeTest, DisallowsAttackedKingRecapture) {
    Board b("8/8/4k3/3p4/3Q4/8/8/K2R4 w - - 0 1");
    EXPECT_EQ(b.see(Move(D4, D5)), eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, AccountsForCapturePromotion) {
    Board b("4k2r/6P1/8/8/8/8/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(G7, H8, MOVE_PROM, QUEEN)),
              eval::piece(ROOK).mg + eval::piece(QUEEN).mg - eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, EnPassantUsesPawnVictim) {
    Board b(board_test::fen::legal_en_passant_a3);
    EXPECT_EQ(b.see(Move(B4, A3, MOVE_EP)), eval::piece(PAWN).mg);
}
