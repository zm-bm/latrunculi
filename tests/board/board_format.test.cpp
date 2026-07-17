#include "board/board_format.hpp"

#include <format>

#include <gtest/gtest.h>

#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"

TEST(BoardFormatTest, PreservesBoardDiagnosticOutput) {
    const board_test::Harness board(board_test::fen::kings_only);

    EXPECT_EQ(std::format("{}", static_cast<const Board&>(board)),
              "   +---+---+---+---+---+---+---+---+\n"
              "   |   |   |   |   | k |   |   |   | 8\n"
              "   +---+---+---+---+---+---+---+---+\n"
              "   |   |   |   |   |   |   |   |   | 7\n"
              "   +---+---+---+---+---+---+---+---+\n"
              "   |   |   |   |   |   |   |   |   | 6\n"
              "   +---+---+---+---+---+---+---+---+\n"
              "   |   |   |   |   |   |   |   |   | 5\n"
              "   +---+---+---+---+---+---+---+---+\n"
              "   |   |   |   |   |   |   |   |   | 4\n"
              "   +---+---+---+---+---+---+---+---+\n"
              "   |   |   |   |   |   |   |   |   | 3\n"
              "   +---+---+---+---+---+---+---+---+\n"
              "   |   |   |   |   |   |   |   |   | 2\n"
              "   +---+---+---+---+---+---+---+---+\n"
              "   |   |   |   |   | K |   |   |   | 1\n"
              "   +---+---+---+---+---+---+---+---+\n"
              "     a   b   c   d   e   f   g   h\n\n"
              "FEN: 4k3/8/8/8/8/8/8/4K3 w - - 0 1\n");
}
