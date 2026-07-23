#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"
#include "support/board_snapshot.hpp"

namespace {

constexpr std::array<std::string_view, 13> round_trip_fens = {
    board_test::fen::start,
    board_test::fen::perft_position_2,
    board_test::fen::perft_position_3,
    board_test::fen::perft_position_4_white,
    board_test::fen::perft_position_4_black,
    board_test::fen::perft_position_5,
    board_test::fen::perft_position_6,
    board_test::fen::kings_only,
    board_test::fen::legal_en_passant_a3,
    board_test::fen::after_e2e4,
    board_test::fen::unhashable_en_passant_e3,
    board_test::fen::en_passant_d6_with_clocks,
    std::string_view{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 255 300"},
};

void expect_same_reloaded_state(std::string_view fen) {
    board_test::Harness board(fen);
    board_test::Harness reloaded(board.to_fen());
    board_test::expect_same_board_snapshot(reloaded, board_test::snapshot_board(board));
    EXPECT_EQ(reloaded.key(), reloaded.calculate_key());
}

} // namespace

TEST(BoardFenTest, LoadsAndOutputsCorrectFens) {
    for (const std::string_view fen : round_trip_fens) {
        SCOPED_TRACE(fen);
        EXPECT_EQ(board_test::Harness(fen).to_fen(), fen) << "should return identical fen";
    }
}

TEST(BoardFenTest, FourFieldFenNormalizesClocks) {
    EXPECT_EQ(board_test::Harness("4k3/8/8/8/8/8/8/4K3 w - -").to_fen(),
              board_test::fen::kings_only);
    EXPECT_EQ(board_test::Harness("4k3/8/8/8/8/8/8/4K3 b - -").to_fen(),
              "4k3/8/8/8/8/8/8/4K3 b - - 0 1");
}

TEST(BoardFenTest, MaxHalfmoveAndLongFullmoveFensRoundTrip) {
    const std::string white = board_test::fen::max_halfmove_long_fullmove;
    const std::string black = "4k3/8/8/8/8/8/8/4K3 b - - 255 300";

    EXPECT_EQ(board_test::Harness(white).to_fen(), white);
    EXPECT_EQ(board_test::Harness(black).to_fen(), black);
    EXPECT_EQ(+board_test::Harness(white).halfmove(), 255);
    EXPECT_EQ(board_test::Harness(white).fullmove(), 300);
}

TEST(BoardFenTest, InvalidFenDoesNotMutateBoard) {
    board_test::Harness board(board_test::fen::after_e2e4);
    const auto          before = board_test::snapshot_board(board);

    const std::vector<std::string> invalid_fens = {
        "4k3/8/8/8/8/8/8/4K3 w - - 0",
        "4k03/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 x - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w QK - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - e3 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 256 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 2147483649",
    };

    for (const auto& invalid_fen : invalid_fens) {
        SCOPED_TRACE(invalid_fen);
        EXPECT_THROW(board.load_fen(invalid_fen), std::invalid_argument) << invalid_fen;
        board_test::expect_same_board_snapshot(board, before);
        EXPECT_EQ(board.key(), board.calculate_key());
    }
}

TEST(BoardFenTest, ReloadingFenReproducesLoadedPosition) {
    for (const std::string_view fen : round_trip_fens) {
        SCOPED_TRACE(fen);
        expect_same_reloaded_state(fen);
    }
}
