#include "fen.hpp"

#include <stdexcept>

#include "test_util.hpp"
#include "gtest/gtest.h"

TEST(FenParserTests, InitialPosition) {
    FenParser parser(STARTFEN);
    EXPECT_EQ(parser.pieces.size(), 32);
    EXPECT_EQ(parser.turn, WHITE);
    EXPECT_EQ(parser.castle, ALL_CASTLE);
    EXPECT_EQ(parser.enpassant, INVALID);
    EXPECT_EQ(parser.halfmove_clk, 0);
    EXPECT_EQ(parser.fullmove_clk, 0);
}

TEST(FenParserTests, EmptyFen) {
    FenParser parser(EMPTYFEN);
    EXPECT_EQ(parser.pieces.size(), 2);
    EXPECT_EQ(parser.turn, WHITE);
    EXPECT_EQ(parser.castle, NO_CASTLE);
    EXPECT_EQ(parser.enpassant, INVALID);
    EXPECT_EQ(parser.halfmove_clk, 0);
    EXPECT_EQ(parser.fullmove_clk, 0);
}

TEST(FenParserTests, EnPassantSquareAndClocks) {
    std::string fen = "8/8/8/3pP3/8/8/8/8 b - e6 10 20";
    FenParser   parser(fen);
    EXPECT_EQ(parser.pieces.size(), 2);
    EXPECT_EQ(parser.turn, BLACK);
    EXPECT_EQ(parser.enpassant, E6);
    EXPECT_EQ(parser.halfmove_clk, 10);
    EXPECT_EQ(parser.fullmove_clk, 39); // For black, 2 * (20 - 1) + 1 = 39.
}

TEST(FenParserTests, InvalidFEN) {
    std::string fen = "invalid fen string";
    EXPECT_THROW(FenParser parser(fen), std::invalid_argument);
}
