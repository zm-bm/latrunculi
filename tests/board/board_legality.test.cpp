#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>

#include "movegen/movegen.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"

TEST(BoardLegalityTest, ReportsEnPassantSquare) {
    EXPECT_EQ(board_test::Harness(board_test::fen::legal_en_passant_a3).enpassant_sq(), A3);
}

TEST(BoardLegalityTest, ReportsLegalEnPassantSquare) {
    EXPECT_EQ(board_test::Harness(board_test::fen::legal_en_passant_a3).legal_enpassant_sq(), A3);
}

TEST(BoardLegalityTest, UncapturableEnPassantDoesNotAffectKey) {
    board_test::Harness with_ep(board_test::fen::unhashable_en_passant_e3);
    board_test::Harness without_ep("4k3/8/8/8/4P3/8/8/4K3 b - - 0 1");

    EXPECT_EQ(with_ep.legal_enpassant_sq(), INVALID);
    EXPECT_EQ(with_ep.key(), with_ep.calculate_key());
    EXPECT_EQ(without_ep.key(), without_ep.calculate_key());
    EXPECT_EQ(with_ep.key(), without_ep.key());
}

TEST(BoardLegalityTest, LegalEnPassantAffectsKey) {
    board_test::Harness with_ep(board_test::fen::legal_en_passant_a3);
    board_test::Harness without_ep("4k3/8/8/8/Pp6/8/8/4K3 b - - 0 1");

    EXPECT_EQ(with_ep.legal_enpassant_sq(), A3);
    EXPECT_EQ(with_ep.key(), with_ep.calculate_key());
    EXPECT_EQ(without_ep.key(), without_ep.calculate_key());
    EXPECT_NE(with_ep.key(), without_ep.key());
}

TEST(BoardLegalityTest, PinnedEnPassantIsNotLegal) {
    board_test::Harness b(board_test::fen::pinned_en_passant_e3);
    EXPECT_EQ(b.legal_enpassant_sq(), INVALID);
}

TEST(BoardLegalityTest, PinnedEnPassantDoesNotAffectKey) {
    board_test::Harness with_ep(board_test::fen::pinned_en_passant_e3);
    board_test::Harness without_ep("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - - 0 1");

    EXPECT_EQ(with_ep.legal_enpassant_sq(), INVALID);
    EXPECT_EQ(with_ep.key(), with_ep.calculate_key());
    EXPECT_EQ(without_ep.key(), without_ep.calculate_key());
    EXPECT_EQ(with_ep.key(), without_ep.key());
}

TEST(BoardLegalityTest, GeneratedMovesSkipIllegalEnPassant) {
    board_test::Harness b(board_test::fen::pinned_en_passant_e3);
    auto                movelist = movegen::generate_pseudo_legal(b);

    auto ep_move = std::find(movelist.begin(), movelist.end(), Move(F4, E3, MOVE_EP));
    EXPECT_EQ(ep_move, movelist.end());
}

TEST(BoardLegalityTest, RejectsPinnedMove) {
    EXPECT_FALSE(
        board_test::Harness(board_test::fen::perft_position_3).is_legal_move(Move(B5, B6)));
}

TEST(BoardLegalityTest, RejectsMoveIntoCheck) {
    EXPECT_FALSE(
        board_test::Harness(board_test::fen::perft_position_3).is_legal_move(Move(A5, B6)));
}

TEST(BoardLegalityTest, RejectsNonEvasionWhileInCheck) {
    board_test::Harness b(board_test::fen::check_evasion);

    ASSERT_TRUE(b.is_check());
    EXPECT_FALSE(b.is_legal_move(Move(G1, F3)));
    EXPECT_TRUE(b.is_legal_move(Move(C1, E3)));
}

TEST(BoardLegalityTest, AcceptsLegalCastle) {
    EXPECT_TRUE(board_test::Harness(board_test::fen::perft_position_2)
                    .is_legal_move(Move(E1, G1, MOVE_CASTLE)));
}

TEST(BoardLegalityTest, AcceptsLegalEnPassant) {
    EXPECT_TRUE(board_test::Harness(board_test::fen::legal_en_passant_a3)
                    .is_legal_move(Move(B4, A3, MOVE_EP)));
}

TEST(BoardLegalityTest, RejectsPinnedEnPassant) {
    board_test::Harness b(board_test::fen::pinned_en_passant_e3);
    EXPECT_FALSE(b.is_legal_move(Move(F4, E3, MOVE_EP)));
}

TEST(BoardLegalityTest, PseudoLegalValidationFiltersGeneratedMoves) {
    EXPECT_FALSE(
        board_test::Harness(board_test::fen::perft_position_3).is_legal_pseudo_move(Move(B5, B6)));
    EXPECT_FALSE(
        board_test::Harness(board_test::fen::perft_position_3).is_legal_pseudo_move(Move(A5, B6)));
    EXPECT_TRUE(board_test::Harness(board_test::fen::legal_en_passant_a3)
                    .is_legal_pseudo_move(Move(B4, A3, MOVE_EP)));
}

TEST(BoardLegalityTest, GeneratedMoveLegalityMatchesFullFilter) {
    for (const std::string_view fen :
         std::array<std::string_view, 9>{board_test::fen::start,
                                         board_test::fen::perft_position_2,
                                         board_test::fen::perft_position_3,
                                         board_test::fen::perft_position_4_white,
                                         board_test::fen::perft_position_4_black,
                                         board_test::fen::legal_en_passant_a3,
                                         board_test::fen::pinned_en_passant_e3,
                                         board_test::fen::check_evasion,
                                         "7k/P7/8/8/8/8/8/4K3 w - - 0 1"}) {
        board_test::Harness board{fen};
        auto                movelist = movegen::generate_pseudo_legal(board);
        for (const Move& move : movelist) {
            EXPECT_EQ(board.is_legal_generated_move(move), board.is_legal_pseudo_move(move))
                << fen << " " << move;
        }
    }
}

TEST(BoardLegalityTest, GeneratedMovesArePseudoLegal) {
    for (const std::string_view fen :
         std::array<std::string_view, 7>{board_test::fen::start,
                                         board_test::fen::perft_position_2,
                                         board_test::fen::perft_position_3,
                                         board_test::fen::perft_position_4_white,
                                         board_test::fen::perft_position_4_black,
                                         board_test::fen::legal_en_passant_a3,
                                         "7k/P7/8/8/8/8/8/4K3 w - - 0 1"}) {
        board_test::Harness board{fen};
        auto                movelist = movegen::generate_pseudo_legal(board);
        for (const Move& move : movelist)
            EXPECT_TRUE(board.is_pseudo_legal(move)) << fen << " " << move;
    }
}

TEST(BoardLegalityTest, RejectsUntrustedNonMoves) {
    board_test::Harness quiet_control{board_test::fen::quiet_black_to_move};
    Move                empty_square_move{H1, H2};

    EXPECT_FALSE(quiet_control.is_legal_move(empty_square_move));
    EXPECT_FALSE(quiet_control.is_pseudo_legal(empty_square_move));

    board_test::Harness start{board_test::fen::start};
    EXPECT_FALSE(start.is_legal_move(NULL_MOVE));
    EXPECT_FALSE(start.is_legal_move(Move(E7, E5)));
    EXPECT_FALSE(start.is_legal_move(Move(B1, D2)));
    EXPECT_FALSE(start.is_legal_move(Move(G1, F3, MOVE_PROM, QUEEN)));
    EXPECT_FALSE(start.is_legal_move(Move(E2, E4, MOVE_CASTLE)));
    EXPECT_FALSE(start.is_pseudo_legal(NULL_MOVE));
    EXPECT_FALSE(start.is_pseudo_legal(Move(E7, E5)));
    EXPECT_FALSE(start.is_pseudo_legal(Move(B1, D2)));
    EXPECT_FALSE(start.is_pseudo_legal(Move(G1, F3, MOVE_PROM, QUEEN)));
    EXPECT_FALSE(start.is_pseudo_legal(Move(E2, E4, MOVE_CASTLE)));
}

TEST(BoardLegalityTest, ValidatesPawnMoveEncoding) {
    board_test::Harness start{board_test::fen::start};
    EXPECT_TRUE(start.is_pseudo_legal(Move(E2, E4)));
    EXPECT_FALSE(start.is_pseudo_legal(Move(E2, E5)));

    board_test::Harness blocked_double_push{"4k3/8/8/8/8/4N3/4P3/4K3 w - - 0 1"};
    EXPECT_FALSE(blocked_double_push.is_pseudo_legal(Move(E2, E4)));

    board_test::Harness promotion{"7k/P7/8/8/8/8/8/4K3 w - - 0 1"};
    EXPECT_FALSE(promotion.is_pseudo_legal(Move(A7, A8)));
    EXPECT_TRUE(promotion.is_pseudo_legal(Move(A7, A8, MOVE_PROM, QUEEN)));

    board_test::Harness enpassant{board_test::fen::legal_en_passant_a3};
    EXPECT_TRUE(enpassant.is_pseudo_legal(Move(B4, A3, MOVE_EP)));
    EXPECT_FALSE(enpassant.is_pseudo_legal(Move(B4, A3)));

    board_test::Harness no_enpassant{"4k3/8/8/8/Pp6/8/8/4K3 b - - 0 1"};
    EXPECT_FALSE(no_enpassant.is_pseudo_legal(Move(B4, A3, MOVE_EP)));
}

TEST(BoardLegalityTest, ValidatesCastlingConditions) {
    board_test::Harness castle_board{board_test::fen::perft_position_2};
    EXPECT_TRUE(castle_board.is_pseudo_legal(Move(E1, G1, MOVE_CASTLE)));
    EXPECT_TRUE(castle_board.is_pseudo_legal(Move(E1, C1, MOVE_CASTLE)));
    EXPECT_FALSE(castle_board.is_pseudo_legal(Move(E1, G1)));

    board_test::Harness no_rights{"r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1"};
    EXPECT_FALSE(no_rights.is_pseudo_legal(Move(E1, G1, MOVE_CASTLE)));

    board_test::Harness blocked_path{board_test::fen::start};
    EXPECT_FALSE(blocked_path.is_pseudo_legal(Move(E1, G1, MOVE_CASTLE)));

    board_test::Harness attacked_path{"r3k2r/8/8/8/8/8/5r2/R3K2R w KQkq - 0 1"};
    EXPECT_FALSE(attacked_path.is_pseudo_legal(Move(E1, G1, MOVE_CASTLE)));
}
