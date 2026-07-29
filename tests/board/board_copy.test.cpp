#include "board/board.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <type_traits>

#include "core/constants.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"
#include "support/board_snapshot.hpp"

namespace {

void complete_repetition_cycle(board_test::Harness& board) {
    board.make(Move(E6, F5));
    board.make(Move(H7, G8));
    board.make(Move(F5, E6));
    board.make(Move(G8, H7));
}

void build_long_history(board_test::Harness& board) {
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

void reverse_long_history(board_test::Harness& board) {
    constexpr int null_moves_per_segment = 200;

    board.unmake();
    for (int ply = 0; ply < null_moves_per_segment; ++ply)
        board.unmake_null();

    board.unmake();
    for (int ply = 0; ply < null_moves_per_segment; ++ply)
        board.unmake_null();

    board.unmake();
    for (int ply = 0; ply < null_moves_per_segment; ++ply)
        board.unmake_null();
}

} // namespace

static_assert(std::is_copy_constructible_v<Board>);
static_assert(std::is_copy_assignable_v<Board>);
static_assert(!std::is_move_constructible_v<Board>);
static_assert(!std::is_move_assignable_v<Board>);

TEST(BoardCopyTest, CopyAssignmentPreservesCompleteIndependentHistory) {
    constexpr std::string_view source_fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 17 42";
    board_test::Harness        source(source_fen);
    source.make(Move(A1, A8));
    const auto expected = board_test::snapshot_board(source);

    Board destination(board_test::fen::start);
    destination.make(Move(G1, F3));

    destination = source;

    EXPECT_TRUE(destination.can_unmake());
    EXPECT_TRUE(source.can_unmake());
    board_test::expect_same_board_snapshot(destination, expected);
    EXPECT_EQ(destination.key(), destination.recompute_key());

    destination.unmake();
    EXPECT_FALSE(destination.can_unmake());
    EXPECT_EQ(destination.to_fen(), source_fen);
    board_test::expect_same_board_snapshot(source, expected);
}

TEST(BoardCopyTest, CopyConstructionPreservesRepresentativeStates) {
    constexpr std::string_view positions[] = {
        board_test::fen::legal_en_passant_a3,
        "4k3/8/8/8/8/8/8/4K3 b - - 73 42",
    };

    for (const std::string_view fen : positions) {
        SCOPED_TRACE(fen);
        board_test::Harness source(fen);
        const auto          expected = board_test::snapshot_board(source);

        Board destination(source);

        board_test::expect_same_board_snapshot(destination, expected);
        EXPECT_EQ(destination.key(), destination.recompute_key());
    }
}

TEST(BoardCopyTest, SourceAndDestinationRemainIndependent) {
    board_test::Harness source(board_test::fen::perft_position_2);
    const auto          root = board_test::snapshot_board(source);

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
    board_test::Harness repeated(board_test::fen::repetition_cycle);
    complete_repetition_cycle(repeated);
    complete_repetition_cycle(repeated);
    repeated.make(Move(E6, F5));
    ASSERT_TRUE(repeated.is_draw());
    const auto repeated_snapshot = board_test::snapshot_board(repeated);

    board_test::Harness fen_only(repeated.to_fen());
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

TEST(BoardCopyTest, SelfAssignmentPreservesPositionAndHistory) {
    Board board(board_test::fen::repetition_cycle);
    complete_repetition_cycle(board);
    const auto before = board_test::snapshot_board(board);

    board = board;

    board_test::expect_same_board_snapshot(board, before);
    EXPECT_TRUE(board.can_unmake());
    board.unmake();
    EXPECT_TRUE(board.can_unmake());
}

TEST(BoardCopyTest, SupportsLongHistoryCopiesAndSearchRoundTrips) {
    board_test::Harness source(board_test::fen::start);
    const auto          initial = board_test::snapshot_board(source);
    build_long_history(source);
    const auto root = board_test::snapshot_board(source);

    Board destination;
    for (int ply = 0; ply < engine::max_search_ply + 8; ++ply)
        destination.make_null();
    ASSERT_TRUE(destination.can_unmake());

    destination = source;
    EXPECT_TRUE(destination.can_unmake());

    for (int ply = 0; ply < engine::max_search_ply; ++ply)
        destination.make_null();
    for (int ply = engine::max_search_ply; ply > 0; --ply)
        destination.unmake_null();

    EXPECT_TRUE(destination.can_unmake());
    board_test::expect_same_board_snapshot(destination, root);
    EXPECT_EQ(destination.key(), destination.recompute_key());

    reverse_long_history(destination);
    board_test::expect_same_board_snapshot(destination, initial);

    board_test::expect_same_board_snapshot(source, root);
    reverse_long_history(source);
    board_test::expect_same_board_snapshot(source, initial);
    EXPECT_EQ(source.key(), source.recompute_key());
}
