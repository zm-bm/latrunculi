#include "board/fen.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/board_fixtures.hpp"

TEST(FenTest, ParsesInitialPosition) {
    const ParsedFen parsed = parse_fen(board_test::fen::start);
    EXPECT_EQ(parsed.pieces.size(), 32);
    EXPECT_EQ(parsed.turn, WHITE);
    EXPECT_EQ(parsed.castle, ALL_CASTLE);
    EXPECT_EQ(parsed.enpassant_target, INVALID);
    EXPECT_EQ(parsed.halfmove_clk, 0);
    EXPECT_EQ(parsed.absolute_ply, 0);
}

TEST(FenTest, ParsesKingsOnlyPosition) {
    const ParsedFen parsed = parse_fen(board_test::fen::kings_only);
    EXPECT_EQ(parsed.pieces.size(), 2);
    EXPECT_EQ(parsed.turn, WHITE);
    EXPECT_EQ(parsed.castle, NO_CASTLE);
    EXPECT_EQ(parsed.enpassant_target, INVALID);
    EXPECT_EQ(parsed.halfmove_clk, 0);
    EXPECT_EQ(parsed.absolute_ply, 0);
}

TEST(FenTest, ParsesEnPassantTargetAndClocks) {
    const ParsedFen parsed = parse_fen(board_test::fen::en_passant_d6_with_clocks);
    EXPECT_EQ(parsed.pieces.size(), 4);
    EXPECT_EQ(parsed.turn, WHITE);
    EXPECT_EQ(parsed.enpassant_target, D6);
    EXPECT_EQ(parsed.halfmove_clk, 10);
    EXPECT_EQ(parsed.absolute_ply, 38);
}

TEST(FenTest, FourFieldFenDefaultsClocks) {
    const ParsedFen parsed = parse_fen("4k3/8/8/8/8/8/8/4K3 b - -");
    EXPECT_EQ(parsed.turn, BLACK);
    EXPECT_EQ(parsed.halfmove_clk, 0);
    EXPECT_EQ(parsed.absolute_ply, 1);
}

TEST(FenTest, ParsesClockBounds) {
    const ParsedFen parsed = parse_fen(board_test::fen::max_halfmove_long_fullmove);
    EXPECT_EQ(+parsed.halfmove_clk, 255);
    EXPECT_EQ(parsed.absolute_ply, 598);
}

TEST(FenTest, RejectsInvalidFen) {
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
        EXPECT_THROW(parse_fen(fen), std::invalid_argument) << fen;
    }
}
