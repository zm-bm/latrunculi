#include "board/board_notation.hpp"

#include <string_view>

#include <gtest/gtest.h>

#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"

namespace {

void expect_san(std::string_view fen, Move move, std::string_view expected) {
    SCOPED_TRACE(fen);
    const board_test::Harness board(fen);
    EXPECT_EQ(to_san(board, move), expected);
}

} // namespace

TEST(BoardNotationTest, FormatsOrdinaryMoves) {
    expect_san(board_test::fen::start, Move(E2, E4), "e4");
    expect_san(board_test::fen::start, Move(G1, F3), "Nf3");
    expect_san("4k3/8/8/8/8/8/8/R3K3 w - - 0 1", Move(A1, A8), "Ra8+");
}

TEST(BoardNotationTest, FormatsCastling) {
    expect_san("5k2/8/8/8/8/8/8/4K2R w K - 0 1", Move(E1, G1, MOVE_CASTLE), "O-O+");
    expect_san("4k3/8/8/8/8/8/8/R3K3 w Q - 0 1", Move(E1, C1, MOVE_CASTLE), "O-O-O");
}

TEST(BoardNotationTest, FormatsCaptures) {
    expect_san("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1", Move(E4, D5), "exd5");
    expect_san("n7/7k/8/8/8/8/8/R3K3 w - - 0 1", Move(A1, A8), "Rxa8");
    expect_san(board_test::fen::legal_en_passant_a3, Move(B4, A3, MOVE_EP), "bxa3");
}

TEST(BoardNotationTest, DisambiguatesByFileRankAndSquare) {
    constexpr std::string_view knights = "4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1";
    constexpr std::string_view rooks   = "7k/8/8/8/8/R7/8/R3K3 w - - 0 1";

    expect_san(knights, Move(B1, D2), "Nbd2");
    expect_san(knights, Move(F1, D2), "Nfd2");
    expect_san(rooks, Move(A1, A2), "R1a2");
    expect_san(rooks, Move(A3, A2), "R3a2");
    expect_san("4k3/8/8/8/8/1N6/8/1N2KN2 w - - 0 1", Move(B1, D2), "Nb1d2");
}

TEST(BoardNotationTest, IgnoresIllegalMoveWhenDisambiguating) {
    expect_san("4r1k1/8/8/8/8/5N2/4N3/4K3 w - - 0 1", Move(F3, D4), "Nd4");
}

TEST(BoardNotationTest, FormatsPromotions) {
    expect_san("7k/P7/8/8/8/8/8/4K3 w - - 0 1", Move(A7, A8, MOVE_PROM, QUEEN), "a8=Q+");
    expect_san(board_test::fen::capture_promotion, Move(A7, B8, MOVE_PROM, QUEEN), "axb8=Q+");
}
