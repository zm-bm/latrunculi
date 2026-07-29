#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/constants.hpp"
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
    EXPECT_EQ(reloaded.key(), reloaded.recompute_key());
}

void complete_repetition_cycle(Board& board) {
    board.make(Move(E6, F5));
    board.make(Move(H7, G8));
    board.make(Move(F5, E6));
    board.make(Move(G8, H7));
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
    EXPECT_EQ(+board_test::Harness(white).halfmove_clock(), 255);
    EXPECT_EQ(board_test::Harness(white).fullmove_number(), 300);
}

TEST(BoardFenTest, ReloadingFenReproducesLoadedPosition) {
    for (const std::string_view fen : round_trip_fens) {
        SCOPED_TRACE(fen);
        expect_same_reloaded_state(fen);
    }
}

TEST(BoardFenTest, LoadingFenReplacesTraversalAndRepetitionHistory) {
    Board board(board_test::fen::repetition_cycle);
    complete_repetition_cycle(board);
    complete_repetition_cycle(board);
    board.make(Move(E6, F5));
    ASSERT_TRUE(board.can_unmake());
    ASSERT_TRUE(board.is_draw());

    const Board expected(board_test::fen::start);
    board.load_fen(board_test::fen::start);

    EXPECT_FALSE(board.can_unmake());
    EXPECT_TRUE(board.previous_move().is_null());
    EXPECT_FALSE(board.is_draw());
    board_test::expect_same_board_snapshot(board, board_test::snapshot_board(expected));
}

TEST(BoardFenTest, InvalidFenLeavesBoardAndTraversalUnchanged) {
    Board board(board_test::fen::perft_position_2);
    board.make(Move(E1, G1, MOVE_CASTLE));
    const auto before = board_test::snapshot_board(board);

    EXPECT_THROW(board.load_fen("invalid fen"), std::invalid_argument);

    EXPECT_TRUE(board.can_unmake());
    board_test::expect_same_board_snapshot(board, before);
    board.unmake();
    EXPECT_FALSE(board.can_unmake());
}

TEST(BoardFenTest, LoadingFenRebindsRootAfterStateStorageGrowth) {
    Board board;
    for (int ply = 0; ply < engine::max_search_ply + 8; ++ply)
        board.make_null();
    ASSERT_TRUE(board.can_unmake());

    board.load_fen(board_test::fen::start);
    const auto root = board_test::snapshot_board(board);
    EXPECT_FALSE(board.can_unmake());

    board.make(Move(E2, E4));
    board.unmake();

    EXPECT_FALSE(board.can_unmake());
    board_test::expect_same_board_snapshot(board, root);
}
