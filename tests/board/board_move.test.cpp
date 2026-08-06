#include "board/board.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string_view>
#include <vector>

#include "core/constants.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_snapshot.hpp"

namespace {

void expect_move_round_trip(std::string_view before, Move move, std::string_view after) {
    Board      board(before);
    const auto before_snapshot = board_test::snapshot_board(board);

    board.make(move);
    EXPECT_EQ(board.to_fen(), after);
    EXPECT_EQ(board.key(), board.recompute_key());
    board_test::expect_evaluation_base_consistent(board);

    board.unmake();
    board_test::expect_same_board_snapshot(board, before_snapshot);
}

} // namespace

TEST(BoardMoveTest, MakesAndUnmakesOrdinaryMovesInLifoOrder) {
    Board      board(board_test::fen::start);
    const auto root = board_test::snapshot_board(board);
    EXPECT_FALSE(board.can_unmake());
    EXPECT_TRUE(board.previous_move().is_null());

    const Move first(G1, F3);
    board.make(first);
    EXPECT_EQ(board.to_fen(), "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1");
    EXPECT_EQ(board.key(), board.recompute_key());
    board_test::expect_evaluation_base_consistent(board);
    EXPECT_TRUE(board.can_unmake());
    EXPECT_EQ(board.previous_move(), first);
    const auto after_first = board_test::snapshot_board(board);

    const Move second(G8, F6);
    board.make(second);
    EXPECT_EQ(board.previous_move(), second);
    EXPECT_EQ(board.key(), board.recompute_key());
    board_test::expect_evaluation_base_consistent(board);

    board.unmake();
    board_test::expect_same_board_snapshot(board, after_first);

    board.unmake();
    board_test::expect_same_board_snapshot(board, root);
}

TEST(BoardMoveTest, MakesAndUnmakesCapture) {
    expect_move_round_trip(board_test::fen::perft_position_2,
                           Move(E2, A6),
                           "r3k2r/p1ppqpb1/Bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1");
}

TEST(BoardMoveTest, UpdatesCastlingRights) {
    struct Case {
        std::string_view before;
        Move             move;
        std::string_view after;
    };

    constexpr std::array cases = {
        Case{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1 1",
             Move(A1, A8),
             "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1"},
        Case{board_test::fen::perft_position_2,
             Move(E1, D1),
             "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R2K3R b kq - 1 1"},
        Case{board_test::fen::perft_position_2,
             Move(H1, F1),
             "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3KR2 b Qkq - 1 1"},
        Case{board_test::fen::perft_position_2,
             Move(A1, C1),
             "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 1 1"},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(testing::Message() << test.before << " move=" << test.move);
        expect_move_round_trip(test.before, test.move, test.after);
    }
}

TEST(BoardMoveTest, DoublePushUpdatesEnPassantState) {
    struct Case {
        std::string_view before;
        Move             move;
        std::string_view after;
        Square           enpassant_target;
        Square           legal_enpassant_target;
    };

    constexpr std::array cases = {
        Case{"4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1",
             Move(D7, D5),
             "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2",
             D6,
             D6},
        Case{board_test::fen::white_pawn_e2,
             Move(E2, E4),
             "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1",
             E3,
             INVALID},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(test.before);
        Board      board(test.before);
        const auto before = board_test::snapshot_board(board);

        board.make(test.move);
        EXPECT_EQ(board.enpassant_target(), test.enpassant_target);
        EXPECT_EQ(board.legal_enpassant_target(), test.legal_enpassant_target);
        EXPECT_EQ(board.to_fen(), test.after);
        EXPECT_EQ(board.key(), board.recompute_key());
        board_test::expect_evaluation_base_consistent(board);

        board.unmake();
        board_test::expect_same_board_snapshot(board, before);
    }
}

TEST(BoardMoveTest, MakesAndUnmakesEnPassantCapture) {
    Board      board(board_test::fen::legal_en_passant_a3);
    const auto before = board_test::snapshot_board(board);

    board.make(Move(B4, A3, MOVE_EP));
    EXPECT_EQ(board.to_fen(), "4k3/8/8/8/8/p7/8/4K3 w - - 0 2");
    EXPECT_EQ(board.legal_enpassant_target(), INVALID);
    EXPECT_EQ(board.key(), board.recompute_key());
    board_test::expect_evaluation_base_consistent(board);

    board.unmake();
    board_test::expect_same_board_snapshot(board, before);
}

TEST(BoardMoveTest, MakesAndUnmakesCastling) {
    struct Case {
        Move             move;
        std::string_view after;
    };

    constexpr std::array cases = {
        Case{Move(E1, G1, MOVE_CASTLE),
             "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R4RK1 b kq - 1 1"},
        Case{Move(E1, C1, MOVE_CASTLE),
             "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2KR3R b kq - 1 1"},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(test.move);
        expect_move_round_trip(board_test::fen::perft_position_2, test.move, test.after);
    }
}

TEST(BoardMoveTest, MakesAndUnmakesPromotions) {
    struct Case {
        std::string_view before;
        Move             move;
        std::string_view after;
    };

    constexpr std::array cases = {
        Case{board_test::fen::white_pawn_on_a7,
             Move(A7, A8, MOVE_PROM, QUEEN),
             "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1"},
        Case{board_test::fen::white_pawn_on_a7,
             Move(A7, A8, MOVE_PROM, BISHOP),
             "B3k3/8/8/8/8/8/8/4K3 b - - 0 1"},
        Case{board_test::fen::capture_promotion,
             Move(A7, B8, MOVE_PROM, QUEEN),
             "1Q2k3/8/8/8/8/8/8/4K3 b - - 0 1"},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(testing::Message() << test.before << " move=" << test.move);
        expect_move_round_trip(test.before, test.move, test.after);
    }
}

TEST(BoardMoveTest, MakesAndUnmakesNullMove) {
    constexpr std::string_view fens[] = {board_test::fen::start,
                                         board_test::fen::perft_position_2,
                                         board_test::fen::legal_en_passant_a3,
                                         board_test::fen::unhashable_en_passant_e3};

    for (const std::string_view fen : fens) {
        SCOPED_TRACE(fen);
        Board      board(fen);
        const auto before          = board_test::snapshot_board(board);
        const auto side_to_move    = board.side_to_move();
        const auto halfmove_clock  = board.halfmove_clock();
        const auto fullmove_number = board.fullmove_number();

        board.make_null();

        EXPECT_EQ(board.occupancy(), before.occupancy);
        EXPECT_EQ(board.evaluation_base(), before.evaluation_base);
        board_test::expect_evaluation_base_consistent(board);
        EXPECT_EQ(board.side_to_move(), ~side_to_move);
        EXPECT_EQ(+board.halfmove_clock(), +halfmove_clock + 1);
        EXPECT_EQ(board.fullmove_number(), fullmove_number);
        EXPECT_EQ(board.enpassant_target(), INVALID);
        EXPECT_EQ(board.legal_enpassant_target(), INVALID);
        EXPECT_TRUE(board.previous_move().is_null());
        EXPECT_TRUE(board.can_unmake());
        EXPECT_EQ(board.key(), board.recompute_key());

        board.unmake_null();
        board_test::expect_same_board_snapshot(board, before);
    }
}

TEST(BoardMoveTest, TraversesAndUnwindsBeyondTheSearchDepthReserve) {
    constexpr std::array cycle = {
        Move(A1, B1),
        Move(H8, G8),
        Move(B1, A1),
        Move(G8, H8),
    };
    constexpr int traversal_plies = engine::max_search_ply + 8;

    Board                                  board(board_test::fen::corner_kings);
    std::vector<board_test::BoardSnapshot> positions;
    positions.reserve(traversal_plies);

    for (int ply = 0; ply < traversal_plies; ++ply) {
        positions.push_back(board_test::snapshot_board(board));
        board.make(cycle[ply % cycle.size()]);
    }

    ASSERT_TRUE(board.can_unmake());
    for (auto position = positions.rbegin(); position != positions.rend(); ++position) {
        board.unmake();
        board_test::expect_same_board_snapshot(board, *position);
    }
    EXPECT_FALSE(board.can_unmake());
}
