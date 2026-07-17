#include <gtest/gtest.h>

#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"

TEST(BoardTacticalStateTest, DirectSliderCheckIsNotPinner) {
    board_test::Harness board("4r2k/8/8/8/8/8/8/4K3 w - - 0 1");

    EXPECT_EQ(board.checkers(), bb::set(E8));
    EXPECT_EQ(board.blockers(WHITE), 0);
    EXPECT_EQ(board.pinners(BLACK), 0);
}

TEST(BoardTacticalStateTest, OwnBlockerSetsBlockerAndPinner) {
    board_test::Harness board("4r2k/8/8/8/8/8/4N3/4K3 w - - 0 1");

    EXPECT_EQ(board.blockers(WHITE), bb::set(E2));
    EXPECT_EQ(board.pinners(BLACK), bb::set(E8));
}

TEST(BoardTacticalStateTest, EnemyBlockerSetsBlockerOnly) {
    board_test::Harness board("4r2k/8/8/8/8/8/4n3/4K3 w - - 0 1");

    EXPECT_EQ(board.blockers(WHITE), bb::set(E2));
    EXPECT_EQ(board.pinners(BLACK), 0);
}

TEST(BoardTacticalStateTest, MultipleSnipersBehindOwnBlockerArePinners) {
    board_test::Harness board(board_test::fen::multiple_pinners);

    EXPECT_EQ(board.blockers(WHITE), bb::set(E2));
    EXPECT_EQ(board.pinners(BLACK), bb::set(E7, E8));
}

TEST(BoardTacticalStateTest, DetectsCheckingMoves) {
    board_test::Harness board(board_test::fen::checking_move_candidates);
    EXPECT_TRUE(board.is_checking_move(Move(A1, A8)));
    EXPECT_TRUE(board.is_checking_move(Move(B1, G6)));
    EXPECT_TRUE(board.is_checking_move(Move(D1, A4)));
    EXPECT_TRUE(board.is_checking_move(Move(G4, F6)));
    EXPECT_FALSE(board.is_checking_move(Move(A1, A7)));
    EXPECT_FALSE(board.is_checking_move(Move(B1, H7)));
    EXPECT_FALSE(board.is_checking_move(Move(D1, F3)));
    EXPECT_FALSE(board.is_checking_move(Move(G4, H6)));
}

TEST(BoardTacticalStateTest, DetectsDiscoveredChecks) {
    board_test::Harness board("Q1N1k3/8/2N1N3/8/B7/8/4R3/4K3 w - - 0 1");
    EXPECT_TRUE(board.is_checking_move(Move(C8, B6)));
    EXPECT_TRUE(board.is_checking_move(Move(C6, B8)));
    EXPECT_TRUE(board.is_checking_move(Move(E6, C5)));
}

TEST(BoardTacticalStateTest, KingMoveDoesNotAlwaysGiveCheck) {
    board_test::Harness board(board_test::fen::kings_only);
    EXPECT_FALSE(board.is_checking_move(Move(E1, D1)));
}

TEST(BoardTacticalStateTest, KingMoveCanDiscoverCheck) {
    board_test::Harness board("4k3/8/8/8/8/8/4K3/4R3 w - - 0 1");
    EXPECT_TRUE(board.is_checking_move(Move(E2, D2)));
}

TEST(BoardTacticalStateTest, EnPassantCanGiveDiscoveredCheck) {
    board_test::Harness board("4k3/8/8/1pP5/B7/8/8/4K3 w - b6 0 1");
    EXPECT_TRUE(board.is_checking_move(Move(C5, B6, MOVE_EP)));
}

TEST(BoardTacticalStateTest, PromotionCanGiveCheck) {
    board_test::Harness board(board_test::fen::white_pawn_on_a7);
    EXPECT_TRUE(board.is_checking_move(Move(A7, A8, MOVE_PROM, QUEEN)));
    EXPECT_TRUE(board.is_checking_move(Move(A7, A8, MOVE_PROM, ROOK)));
    EXPECT_FALSE(board.is_checking_move(Move(A7, A8, MOVE_PROM, BISHOP)));
    EXPECT_FALSE(board.is_checking_move(Move(A7, A8, MOVE_PROM, KNIGHT)));
}

TEST(BoardTacticalStateTest, CastlingCanGiveCheck) {
    board_test::Harness board("5k2/8/8/8/8/8/8/4K2R w K - 0 1");
    EXPECT_TRUE(board.is_checking_move(Move(E1, G1, MOVE_CASTLE)));
}

TEST(BoardTacticalStateTest, IdentifiesCaptures) {
    board_test::Harness board(board_test::fen::perft_position_2);
    EXPECT_TRUE(board.is_capture(Move(D5, E6)));
    EXPECT_TRUE(board.is_capture(Move(F3, F6)));
    EXPECT_TRUE(board_test::Harness(board_test::fen::legal_en_passant_a3)
                    .is_capture(Move(B4, A3, MOVE_EP)));
    EXPECT_FALSE(board.is_capture(Move(A2, A4)));
    EXPECT_FALSE(board.is_capture(Move(C3, B5)));
}

TEST(BoardTacticalStateTest, IdentifiesCapturedPieceType) {
    board_test::Harness board(board_test::fen::perft_position_2);
    EXPECT_EQ(board.captured_piece_type(Move(D5, E6)), PAWN);
    EXPECT_EQ(board.captured_piece_type(Move(A2, A4)), NO_PIECETYPE);
    EXPECT_EQ(board_test::Harness(board_test::fen::legal_en_passant_a3)
                  .captured_piece_type(Move(B4, A3, MOVE_EP)),
              PAWN);
}

TEST(BoardTacticalStateTest, DetectsDoubleCheck) {
    EXPECT_TRUE(board_test::Harness("R3k3/8/8/8/8/8/4Q3/4K3 b - - 0 1").is_double_check());
}

TEST(BoardTacticalStateTest, FindsAttacksFromPinnedPieces) {
    board_test::Harness board(board_test::fen::perft_position_3);
    EXPECT_TRUE(board.attacks_to(bb::file(FILE8), WHITE));
    EXPECT_TRUE(board.attacks_to(bb::file(FILE2), BLACK));
    EXPECT_FALSE(board.attacks_to(bb::file(FILE7), WHITE));
    EXPECT_FALSE(board.attacks_to(bb::file(FILE1), BLACK));
}

TEST(BoardTacticalStateTest, FindsStartPositionAttacks) {
    board_test::Harness board(board_test::fen::start);

    EXPECT_EQ(board.attacks_to(A2, WHITE), bb::set(A1));
    EXPECT_EQ(board.attacks_to(A3, WHITE), bb::set(B2, B1));
    EXPECT_EQ(board.attacks_to(A4, WHITE), 0);
    EXPECT_EQ(board.attacks_to(B2, WHITE), bb::set(C1));
    EXPECT_EQ(board.attacks_to(B3, WHITE), bb::set(A2, C2));
    EXPECT_EQ(board.attacks_to(B4, WHITE), 0);

    EXPECT_TRUE(board.attacks_to(bb::rank(RANK1), WHITE));
    EXPECT_TRUE(board.attacks_to(bb::rank(RANK3), WHITE));
    EXPECT_TRUE(board.attacks_to(bb::rank(RANK8), BLACK));
    EXPECT_TRUE(board.attacks_to(bb::rank(RANK6), BLACK));
    EXPECT_FALSE(board.attacks_to(bb::rank(RANK4), WHITE));
    EXPECT_FALSE(board.attacks_to(bb::rank(RANK5), BLACK));
}

TEST(BoardTacticalStateTest, ValidatesLegalMovesWhileInCheck) {
    board_test::Harness board(board_test::fen::perft_position_4_black);

    EXPECT_TRUE(board.is_check());
    EXPECT_FALSE(board.is_double_check());

    EXPECT_TRUE(board.is_legal_move(Move(G8, H8)));
    EXPECT_TRUE(board.is_legal_move(Move(F8, F7)));
    EXPECT_TRUE(board.is_legal_move(Move(F6, D5)));
    EXPECT_TRUE(board.is_legal_move(Move(D7, D5)));
    EXPECT_TRUE(board.is_legal_move(Move(C5, C4)));
    EXPECT_TRUE(board.is_legal_move(Move(B5, C4)));
    EXPECT_FALSE(board.is_legal_move(Move(G8, F7)));
}
