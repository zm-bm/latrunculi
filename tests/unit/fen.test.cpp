#include "fen.hpp"

#include <stdexcept>

#include "gtest/gtest.h"

TEST(FenParserTests, InitialPosition) {
    FenParser parser(STARTFEN);
    EXPECT_EQ(parser.pieces.size(), 32);
    EXPECT_EQ(parser.turn, WHITE);
    EXPECT_EQ(parser.castle, ALL_CASTLE);
    EXPECT_EQ(parser.enPassantSq, INVALID);
    EXPECT_EQ(parser.hmClock, 0);
    EXPECT_EQ(parser.moveCounter, 0);
}

TEST(FenParserTests, EmptyFen) {
    FenParser parser(EMPTYFEN);
    EXPECT_EQ(parser.pieces.size(), 2);
    EXPECT_EQ(parser.turn, WHITE);
    EXPECT_EQ(parser.castle, NO_CASTLE);
    EXPECT_EQ(parser.enPassantSq, INVALID);
    EXPECT_EQ(parser.hmClock, 0);
    EXPECT_EQ(parser.moveCounter, 0);
}

TEST(FenParserTests, EnPassantSquareAndClocks) {
    std::string fen = "8/8/8/3pP3/8/8/8/8 b - e6 10 20";
    FenParser parser(fen);
    EXPECT_EQ(parser.pieces.size(), 2);
    EXPECT_EQ(parser.turn, BLACK);
    EXPECT_EQ(parser.enPassantSq, E6);
    EXPECT_EQ(parser.hmClock, 10);
    EXPECT_EQ(parser.moveCounter, 39);  // For black, moveCounter = 2 * (20 - 1) + 1 = 39.
}

TEST(FenParserTests, InvalidFEN) {
    std::string fen = "invalid fen string";
    EXPECT_THROW(FenParser parser(fen), std::invalid_argument);
}
