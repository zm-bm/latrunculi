#include "board/fen.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/board_fixtures.hpp"

TEST(FenParserTest, InitialPosition) {
    FenParser parser{std::string(board_test::fen::start)};
    EXPECT_EQ(parser.pieces.size(), 32);
    EXPECT_EQ(parser.turn, WHITE);
    EXPECT_EQ(parser.castle, ALL_CASTLE);
    EXPECT_EQ(parser.enpassant, INVALID);
    EXPECT_EQ(parser.halfmove_clk, 0);
    EXPECT_EQ(parser.fullmove_clk, 0);
}

TEST(FenParserTest, EmptyFen) {
    FenParser parser{std::string(board_test::fen::kings_only)};
    EXPECT_EQ(parser.pieces.size(), 2);
    EXPECT_EQ(parser.turn, WHITE);
    EXPECT_EQ(parser.castle, NO_CASTLE);
    EXPECT_EQ(parser.enpassant, INVALID);
    EXPECT_EQ(parser.halfmove_clk, 0);
    EXPECT_EQ(parser.fullmove_clk, 0);
}

TEST(FenParserTest, EnPassantSquareAndClocks) {
    std::string fen = std::string(board_test::fen::en_passant_d6_with_clocks);
    FenParser   parser(fen);
    EXPECT_EQ(parser.pieces.size(), 4);
    EXPECT_EQ(parser.turn, WHITE);
    EXPECT_EQ(parser.enpassant, D6);
    EXPECT_EQ(parser.halfmove_clk, 10);
    EXPECT_EQ(parser.fullmove_clk, 38);
}

TEST(FenParserTest, FourFieldFenDefaultsClocks) {
    FenParser parser("4k3/8/8/8/8/8/8/4K3 b - -");
    EXPECT_EQ(parser.turn, BLACK);
    EXPECT_EQ(parser.halfmove_clk, 0);
    EXPECT_EQ(parser.fullmove_clk, 1);
}

TEST(FenParserTest, MaxHalfmoveAndLongFullmove) {
    FenParser parser(board_test::fen::max_halfmove_long_fullmove);
    EXPECT_EQ(+parser.halfmove_clk, 255);
    EXPECT_EQ(parser.fullmove_clk, 598);
}

TEST(FenParserTest, InvalidFen) {
    const std::vector<std::string> invalid_fens = {
        "invalid fen string",
        "4k3/8/8/8/8/8/8/4K3 w - - 0",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1 extra",
        "4k3/8/8/8/8/8/4K3 w - - 0 1",
        "9/8/8/8/8/8/8/4K2k w - - 0 1",
        "7/8/8/8/8/8/8/4K2k w - - 0 1",
        "4k03/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K2X w - - 0 1",
        "8/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K2K w - - 0 1",
        "P3k3/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 x - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w QK - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w KZ - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w KK - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - e3 0 1",
        "4k3/8/8/8/8/8/8/4K3 b - e6 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - e9 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - - -1 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 12x 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 256 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 0",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 4294967296",
    };

    for (const auto& fen : invalid_fens) {
        EXPECT_THROW({ FenParser parser(fen); }, std::invalid_argument) << fen;
    }
}
