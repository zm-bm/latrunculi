#include <gtest/gtest.h>

#include <string_view>

#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"

TEST(BoardMoveApplicationTest, MakesAndUnmakesKnightMove) {
    board_test::Harness board(board_test::fen::start);
    board.make(Move(G1, F3));
    EXPECT_EQ(board.to_fen(), "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::start);
}

TEST(BoardMoveApplicationTest, MakesAndUnmakesBishopCapture) {
    board_test::Harness board(board_test::fen::perft_position_2);
    board.make(Move(E2, A6));
    EXPECT_EQ(board.to_fen(),
              "r3k2r/p1ppqpb1/Bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::perft_position_2);
}

TEST(BoardMoveApplicationTest, RookCaptureDisablesCastlingRights) {
    constexpr std::string_view fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1 1";
    board_test::Harness        board(fen);
    board.make(Move(A1, A8));
    EXPECT_EQ(board.to_fen(), "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), fen);
}

TEST(BoardMoveApplicationTest, PawnDoublePushSetsEnPassantSquare) {
    constexpr std::string_view fen = "4k3/8/8/8/1p6/8/P7/4K3 w - - 0 1";
    board_test::Harness        board(fen);
    board.make(Move(A2, A4));
    EXPECT_EQ(board.to_fen(), board_test::fen::legal_en_passant_a3);
    board.unmake();
    EXPECT_EQ(board.to_fen(), fen);
}

TEST(BoardMoveApplicationTest, MakesAndUnmakesEnPassantCapture) {
    board_test::Harness board(board_test::fen::legal_en_passant_a3);
    board.make(Move(B4, A3, MOVE_EP));
    EXPECT_EQ(board.to_fen(), "4k3/8/8/8/8/p7/8/4K3 w - - 0 2");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::legal_en_passant_a3);
}

TEST(BoardMoveApplicationTest, MakesAndUnmakesKingsideCastle) {
    board_test::Harness board(board_test::fen::perft_position_2);
    board.make(Move(E1, G1, MOVE_CASTLE));
    EXPECT_EQ(board.to_fen(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R4RK1 b kq - 1 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::perft_position_2);
}

TEST(BoardMoveApplicationTest, MakesAndUnmakesQueensideCastle) {
    board_test::Harness board(board_test::fen::perft_position_2);
    board.make(Move(E1, C1, MOVE_CASTLE));
    EXPECT_EQ(board.to_fen(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2KR3R b kq - 1 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::perft_position_2);
}

TEST(BoardMoveApplicationTest, KingMoveDisablesCastlingRights) {
    board_test::Harness board(board_test::fen::perft_position_2);
    board.make(Move(E1, D1));
    EXPECT_EQ(board.to_fen(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R2K3R b kq - 1 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::perft_position_2);
}

TEST(BoardMoveApplicationTest, KingsideRookMoveDisablesKingsideCastlingRights) {
    board_test::Harness board(board_test::fen::perft_position_2);
    board.make(Move(H1, F1));
    EXPECT_EQ(board.to_fen(),
              "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3KR2 b Qkq - 1 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::perft_position_2);
}

TEST(BoardMoveApplicationTest, QueensideRookMoveDisablesQueensideCastlingRights) {
    board_test::Harness board(board_test::fen::perft_position_2);
    board.make(Move(A1, C1));
    EXPECT_EQ(board.to_fen(),
              "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 1 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::perft_position_2);
}

TEST(BoardMoveApplicationTest, MakesAndUnmakesQueenPromotion) {
    board_test::Harness board(board_test::fen::white_pawn_on_a7);
    board.make(Move(A7, A8, MOVE_PROM, QUEEN));
    EXPECT_EQ(board.to_fen(), "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::white_pawn_on_a7);
}

TEST(BoardMoveApplicationTest, MakesAndUnmakesUnderpromotion) {
    board_test::Harness board(board_test::fen::white_pawn_on_a7);
    board.make(Move(A7, A8, MOVE_PROM, BISHOP));
    EXPECT_EQ(board.to_fen(), "B3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    board.unmake();
    EXPECT_EQ(board.to_fen(), board_test::fen::white_pawn_on_a7);
}
