#include "board/board.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <type_traits>

#include "core/constants.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_snapshot.hpp"

namespace {

void complete_repetition_cycle(Board& board) {
    board.make(Move(E6, F5));
    board.make(Move(H7, G8));
    board.make(Move(F5, E6));
    board.make(Move(G8, H7));
}

void build_long_history(Board& board) {
    constexpr int null_moves_per_segment = 200;

    for (int ply = 0; ply < null_moves_per_segment; ++ply)
        board.make_null();
    board.make(Move(E2, E4));

    for (int ply = 0; ply < null_moves_per_segment; ++ply)
        board.make_null();
    board.make(Move(E7, E5));

    for (int ply = 0; ply < null_moves_per_segment; ++ply)
        board.make_null();
    board.make(Move(G2, G4));
}

void unwind_history(Board& board) {
    while (board.can_unmake()) {
        if (board.previous_move().is_null())
            board.unmake_null();
        else
            board.unmake();
    }
}

} // namespace

static_assert(std::is_copy_constructible_v<Board>);
static_assert(std::is_copy_assignable_v<Board>);
static_assert(!std::is_move_constructible_v<Board>);
static_assert(!std::is_move_assignable_v<Board>);

TEST(BoardRepresentationTest, LoadsRepresentativePositionState) {
    Board board(board_test::fen::perft_position_4_black);

    EXPECT_EQ(board.side_to_move(), BLACK);
    EXPECT_EQ(board.king_sq(WHITE), E1);
    EXPECT_EQ(board.king_sq(BLACK), G8);
    EXPECT_EQ(board.piece_on(B7), W_PAWN);
    EXPECT_EQ(board.piece_on(A2), B_PAWN);
    EXPECT_EQ(board.piece_on(B3), W_BISHOP);
    EXPECT_EQ(board.piece_on(F6), B_KNIGHT);
    EXPECT_EQ(board.count(WHITE, PAWN), 7);
    EXPECT_EQ(board.count(BLACK, PAWN), 8);
    EXPECT_EQ(board.castling_rights(), W_CASTLE);
    EXPECT_EQ(board.checkers(), bb::set(B3));
    EXPECT_EQ(board.base_terms().material(), eval::piece(PAWN, BLACK));
    EXPECT_LT(board.base_terms().piece_square().mg, 0);
    EXPECT_EQ(board.key(), board.recompute_key());
}

TEST(BoardRepresentationTest, MaterialAndPsqtMatchExpectedValues) {
    Board board("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");

    EXPECT_EQ(board.base_terms().material(), eval::piece(QUEEN, WHITE) + eval::piece(ROOK, BLACK));
    EXPECT_EQ(board.base_terms().piece_square(),
              eval::piece_sq(QUEEN, WHITE, D1) + eval::piece_sq(ROOK, BLACK, D8));
}

TEST(BoardCopyTest, SourceAndDestinationRemainIndependent) {
    Board      source(board_test::fen::perft_position_2);
    const auto root = board_test::snapshot_board(source);

    Board destination(source);

    destination.make(Move(E1, G1, MOVE_CASTLE));
    board_test::expect_same_board_snapshot(source, root);
    destination.unmake();
    board_test::expect_same_board_snapshot(destination, root);

    source.make(Move(E1, C1, MOVE_CASTLE));
    board_test::expect_same_board_snapshot(destination, root);
    source.unmake();
    board_test::expect_same_board_snapshot(source, root);
}

TEST(BoardCopyTest, ReplacesRepetitionHistoryWhenDestinationIsReused) {
    Board repeated(board_test::fen::repetition_cycle);
    complete_repetition_cycle(repeated);
    complete_repetition_cycle(repeated);
    repeated.make(Move(E6, F5));
    ASSERT_TRUE(repeated.is_draw());
    const auto repeated_snapshot = board_test::snapshot_board(repeated);

    Board fen_only(repeated.to_fen());
    EXPECT_FALSE(fen_only.is_draw());
    const auto fen_snapshot = board_test::snapshot_board(fen_only);

    Board destination;

    destination = repeated;
    EXPECT_TRUE(destination.is_draw());
    board_test::expect_same_board_snapshot(destination, repeated_snapshot);

    destination = fen_only;
    EXPECT_FALSE(destination.is_draw());
    board_test::expect_same_board_snapshot(destination, fen_snapshot);

    destination = repeated;
    EXPECT_TRUE(destination.is_draw());
    board_test::expect_same_board_snapshot(destination, repeated_snapshot);
}

TEST(BoardCopyTest, CopiesAndUnwindsHistoryAcrossTheGrowthBoundary) {
    Board      source(board_test::fen::start);
    const auto initial = board_test::snapshot_board(source);
    build_long_history(source);
    const auto root = board_test::snapshot_board(source);

    Board destination;
    for (int ply = 0; ply < engine::max_search_ply + 8; ++ply)
        destination.make_null();

    destination = source;
    for (int ply = 0; ply < engine::max_search_ply; ++ply)
        destination.make_null();
    for (int ply = engine::max_search_ply; ply > 0; --ply)
        destination.unmake_null();

    board_test::expect_same_board_snapshot(destination, root);
    EXPECT_EQ(destination.key(), destination.recompute_key());

    unwind_history(destination);
    board_test::expect_same_board_snapshot(destination, initial);

    board_test::expect_same_board_snapshot(source, root);
    unwind_history(source);
    board_test::expect_same_board_snapshot(source, initial);
    EXPECT_EQ(source.key(), source.recompute_key());
}
