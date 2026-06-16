#include "board.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "movegen.hpp"
#include "test_util.hpp"

TEST(BoardTest, is_stalemate) {
    EXPECT_TRUE(Board("k7/8/KQ6/8/8/8/8/8 b - - 0 1").is_stalemate());
    EXPECT_FALSE(Board("k7/8/KQ6/8/8/8/8/8 w - - 0 1").is_stalemate());
    EXPECT_TRUE(Board("8/8/8/8/8/kq6/8/K7 w - - 0 1").is_stalemate());
    EXPECT_FALSE(Board("8/8/8/8/8/kq6/8/K7 b - - 0 1").is_stalemate());
}

TEST(BoardTest, is_draw_50_move_rule) {
    EXPECT_TRUE(Board("k7/8/8/8/8/8/8/K7 w - - 100 50").is_draw());
    EXPECT_FALSE(Board("k7/8/8/8/8/8/8/K7 w - - 99 50").is_draw());
}

TEST(BoardTest, is_draw_50_3_fold_repetition) {
    Board b("3r4/ppq4k/1nb1BQ1p/4Pp1p/1b6/8/PP3PPP/2R1R1K1 w - - 2 26");

    b.make(Move(E6, F5));
    b.make(Move(H7, G8));
    b.make(Move(F5, E6));
    b.make(Move(G8, H7));
    b.make(Move(E6, F5));
    EXPECT_FALSE(b.is_draw());
    b.make(Move(H7, G8));
    EXPECT_FALSE(b.is_draw());
    b.make(Move(F5, E6));
    EXPECT_FALSE(b.is_draw());
    b.make(Move(G8, H7));
    EXPECT_FALSE(b.is_draw());
    b.make(Move(E6, F5));
    EXPECT_TRUE(b.is_draw());
}

TEST(BoardTest, enpassant_sq) {
    EXPECT_EQ(Board(ENPASSANT_A3).enpassant_sq(), A3);
}

TEST(BoardTest, legal_enpassant_sq) {
    EXPECT_EQ(Board(ENPASSANT_A3).legal_enpassant_sq(), A3);
}

TEST(BoardTest, enpassant_key_ignores_uncapturable_square) {
    Board with_ep("4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1");
    Board without_ep("4k3/8/8/8/4P3/8/8/4K3 b - - 0 1");

    EXPECT_EQ(with_ep.legal_enpassant_sq(), INVALID);
    EXPECT_EQ(with_ep.key(), with_ep.calculate_key());
    EXPECT_EQ(without_ep.key(), without_ep.calculate_key());
    EXPECT_EQ(with_ep.key(), without_ep.key());
}

TEST(BoardTest, enpassant_key_hashes_legal_square) {
    Board with_ep(ENPASSANT_A3);
    Board without_ep("4k3/8/8/8/Pp6/8/8/4K3 b - - 0 1");

    EXPECT_EQ(with_ep.legal_enpassant_sq(), A3);
    EXPECT_EQ(with_ep.key(), with_ep.calculate_key());
    EXPECT_EQ(without_ep.key(), without_ep.calculate_key());
    EXPECT_NE(with_ep.key(), without_ep.key());
}

TEST(BoardTest, legal_enpassant_sq_pinned_en_passant) {
    Board b("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1");
    EXPECT_EQ(b.legal_enpassant_sq(), INVALID);
}

TEST(BoardTest, enpassant_key_ignores_pinned_en_passant) {
    Board with_ep("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1");
    Board without_ep("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - - 0 1");

    EXPECT_EQ(with_ep.legal_enpassant_sq(), INVALID);
    EXPECT_EQ(with_ep.key(), with_ep.calculate_key());
    EXPECT_EQ(without_ep.key(), without_ep.calculate_key());
    EXPECT_EQ(with_ep.key(), without_ep.key());
}

TEST(BoardTest, generated_moves_skip_illegal_en_passant) {
    Board b("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1");
    auto  movelist = generate<ALL_MOVES>(b);

    auto ep_move = std::find(movelist.begin(), movelist.end(), Move(F4, E3, MOVE_EP));
    EXPECT_EQ(ep_move, movelist.end());
}

TEST(BoardTest, halfmove) {
    EXPECT_EQ(Board("4k3/8/8/8/8/8/4P3/4K3 w - - 7 1").halfmove(), 7);
}

TEST(BoardTest, static_exchange_basic) {
    Board b("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -");
    EXPECT_EQ(b.seeMove(Move(E1, E5)), eval::piece(PAWN).mg);
}

TEST(BoardTest, static_exchange_trade) {
    Board b("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
    EXPECT_EQ(b.seeMove(Move(D3, E5)), eval::piece(PAWN).mg - eval::piece(KNIGHT).mg);
}

TEST(BoardTest, static_exchange_prefers_pawn_over_knight_recapturer) {
    Board b("7k/1B1p4/4p3/P2p4/2P5/2n5/8/K7 w - - 0 1");
    EXPECT_EQ(b.seeMove(Move(B7, D5)), eval::piece(PAWN).mg - eval::piece(BISHOP).mg);
}

TEST(BoardTest, static_exchange_handles_king_recapture) {
    Board b("8/8/4k3/3p4/3Q4/8/8/K7 w - - 0 1");
    EXPECT_EQ(b.seeMove(Move(D4, D5)), eval::piece(PAWN).mg - eval::piece(QUEEN).mg);
}

TEST(BoardTest, is_legal_move_pin) {
    EXPECT_FALSE(Board(POS3).is_legal_move(Move(B5, B6)));
}

TEST(BoardTest, is_legal_move_moving_into_check) {
    EXPECT_FALSE(Board(POS3).is_legal_move(Move(A5, B6)));
}

TEST(BoardTest, is_legal_move_rejects_non_evasion_while_in_check) {
    Board b("k3r3/8/8/8/8/8/8/2B1K1N1 w - - 0 1");

    ASSERT_TRUE(b.is_check());
    EXPECT_FALSE(b.is_legal_move(Move(G1, F3)));
    EXPECT_TRUE(b.is_legal_move(Move(C1, E3)));
}

TEST(BoardTest, is_legal_move_castle) {
    EXPECT_TRUE(Board(POS2).is_legal_move(Move(E1, G1, MOVE_CASTLE)));
}

TEST(BoardTest, is_legal_move_en_passant) {
    EXPECT_TRUE(Board(ENPASSANT_A3).is_legal_move(Move(B4, A3, MOVE_EP)));
}

TEST(BoardTest, is_legal_move_pinned_en_passant) {
    Board b("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1");
    EXPECT_FALSE(b.is_legal_move(Move(F4, E3, MOVE_EP)));
}

TEST(BoardTest, is_legal_pseudo_move_filters_generated_moves) {
    EXPECT_FALSE(Board(POS3).is_legal_pseudo_move(Move(B5, B6)));
    EXPECT_FALSE(Board(POS3).is_legal_pseudo_move(Move(A5, B6)));
    EXPECT_TRUE(Board(ENPASSANT_A3).is_legal_pseudo_move(Move(B4, A3, MOVE_EP)));
}

TEST(BoardTest, is_pseudo_legal_accepts_generated_moves) {
    for (const std::string fen :
         {STARTFEN, POS2, POS3, POS4W, POS4B, ENPASSANT_A3, "7k/P7/8/8/8/8/8/4K3 w - - 0 1"}) {
        Board board{fen};
        auto  movelist = generate<ALL_MOVES>(board);
        for (const Move& move : movelist)
            EXPECT_TRUE(board.is_pseudo_legal(move)) << fen << " " << move;
    }
}

TEST(BoardTest, is_pseudo_legal_rejects_untrusted_non_moves) {
    Board quiet_control{"k7/8/2K5/8/8/8/8/8 b - - 0 1"};
    Move  empty_square_move{H1, H2};

    EXPECT_FALSE(quiet_control.is_legal_move(empty_square_move));
    EXPECT_FALSE(quiet_control.is_pseudo_legal(empty_square_move));

    Board start{STARTFEN};
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

TEST(BoardTest, is_pseudo_legal_validates_pawn_move_encoding) {
    Board start{STARTFEN};
    EXPECT_TRUE(start.is_pseudo_legal(Move(E2, E4)));
    EXPECT_FALSE(start.is_pseudo_legal(Move(E2, E5)));

    Board blocked_double_push{"4k3/8/8/8/8/4N3/4P3/4K3 w - - 0 1"};
    EXPECT_FALSE(blocked_double_push.is_pseudo_legal(Move(E2, E4)));

    Board promotion{"7k/P7/8/8/8/8/8/4K3 w - - 0 1"};
    EXPECT_FALSE(promotion.is_pseudo_legal(Move(A7, A8)));
    EXPECT_TRUE(promotion.is_pseudo_legal(Move(A7, A8, MOVE_PROM, QUEEN)));

    Board enpassant{ENPASSANT_A3};
    EXPECT_TRUE(enpassant.is_pseudo_legal(Move(B4, A3, MOVE_EP)));
    EXPECT_FALSE(enpassant.is_pseudo_legal(Move(B4, A3)));

    Board no_enpassant{"4k3/8/8/8/Pp6/8/8/4K3 b - - 0 1"};
    EXPECT_FALSE(no_enpassant.is_pseudo_legal(Move(B4, A3, MOVE_EP)));
}

TEST(BoardTest, is_pseudo_legal_validates_castling_conditions) {
    Board castle_board{POS2};
    EXPECT_TRUE(castle_board.is_pseudo_legal(Move(E1, G1, MOVE_CASTLE)));
    EXPECT_TRUE(castle_board.is_pseudo_legal(Move(E1, C1, MOVE_CASTLE)));
    EXPECT_FALSE(castle_board.is_pseudo_legal(Move(E1, G1)));

    Board no_rights{"r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1"};
    EXPECT_FALSE(no_rights.is_pseudo_legal(Move(E1, G1, MOVE_CASTLE)));

    Board blocked_path{STARTFEN};
    EXPECT_FALSE(blocked_path.is_pseudo_legal(Move(E1, G1, MOVE_CASTLE)));

    Board attacked_path{"r3k2r/8/8/8/8/8/5r2/R3K2R w KQkq - 0 1"};
    EXPECT_FALSE(attacked_path.is_pseudo_legal(Move(E1, G1, MOVE_CASTLE)));
}

TEST(BoardTest, is_checking_move) {
    Board b("4k3/8/8/8/6N1/8/8/RB1QK3 w - - 0 1");
    EXPECT_TRUE(b.is_checking_move(Move(A1, A8)));
    EXPECT_TRUE(b.is_checking_move(Move(B1, G6)));
    EXPECT_TRUE(b.is_checking_move(Move(D1, A4)));
    EXPECT_TRUE(b.is_checking_move(Move(G4, F6)));
    EXPECT_FALSE(b.is_checking_move(Move(A1, A7)));
    EXPECT_FALSE(b.is_checking_move(Move(B1, H7)));
    EXPECT_FALSE(b.is_checking_move(Move(D1, F3)));
    EXPECT_FALSE(b.is_checking_move(Move(G4, H6)));
}

TEST(BoardTest, is_checking_move_discovered_checks) {
    Board b("Q1N1k3/8/2N1N3/8/B7/8/4R3/4K3 w - - 0 1");
    EXPECT_TRUE(b.is_checking_move(Move(C8, B6)));
    EXPECT_TRUE(b.is_checking_move(Move(C6, B8)));
    EXPECT_TRUE(b.is_checking_move(Move(E6, C5)));
}

TEST(BoardTest, is_checking_move_king_move) {
    Board b("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_FALSE(b.is_checking_move(Move(E1, D1)));
}

TEST(BoardTest, is_checking_move_king_move_discovered_check) {
    Board b("4k3/8/8/8/8/8/4K3/4R3 w - - 0 1");
    EXPECT_TRUE(b.is_checking_move(Move(E2, D2)));
}

TEST(BoardTest, is_checking_move_en_passant) {
    Board b("4k3/8/8/1pP5/B7/8/8/4K3 w - b6 0 1");
    EXPECT_TRUE(b.is_checking_move(Move(C5, B6, MOVE_EP)));
}

TEST(BoardTest, is_checking_move_promotion) {
    Board b("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_TRUE(b.is_checking_move(Move(A7, A8, MOVE_PROM, QUEEN)));
    EXPECT_TRUE(b.is_checking_move(Move(A7, A8, MOVE_PROM, ROOK)));
    EXPECT_FALSE(b.is_checking_move(Move(A7, A8, MOVE_PROM, BISHOP)));
    EXPECT_FALSE(b.is_checking_move(Move(A7, A8, MOVE_PROM, KNIGHT)));
}

TEST(BoardTest, is_checking_move_castle) {
    Board b("5k2/8/8/8/8/8/8/4K2R w K - 0 1");
    EXPECT_TRUE(b.is_checking_move(Move(E1, G1, MOVE_CASTLE)));
}

TEST(BoardTest, is_capture) {
    Board b(POS2);
    EXPECT_TRUE(b.is_capture(Move(D5, E6)));
    EXPECT_TRUE(b.is_capture(Move(F3, F6)));
    EXPECT_TRUE(Board(ENPASSANT_A3).is_capture(Move(B4, A3, MOVE_EP)));
    EXPECT_FALSE(b.is_capture(Move(A2, A4)));
    EXPECT_FALSE(b.is_capture(Move(C3, B5)));
}

TEST(BoardTest, is_double_check) {
    EXPECT_TRUE(Board("R3k3/8/8/8/8/8/4Q3/4K3 b - - 0 1").is_double_check());
}

TEST(BoardTest, attacks_to_pinned) {
    Board b(POS3);
    EXPECT_TRUE(b.attacks_to(bb::file(FILE8), WHITE));
    EXPECT_TRUE(b.attacks_to(bb::file(FILE2), BLACK));
    EXPECT_FALSE(b.attacks_to(bb::file(FILE7), WHITE));
    EXPECT_FALSE(b.attacks_to(bb::file(FILE1), BLACK));
}

TEST(BoardTest, start_position_attacks) {
    Board b(STARTFEN);

    EXPECT_EQ(b.attacks_to(A2, WHITE), bb::set(A1));
    EXPECT_EQ(b.attacks_to(A3, WHITE), bb::set(B2, B1));
    EXPECT_EQ(b.attacks_to(A4, WHITE), 0);
    EXPECT_EQ(b.attacks_to(B2, WHITE), bb::set(C1));
    EXPECT_EQ(b.attacks_to(B3, WHITE), bb::set(A2, C2));
    EXPECT_EQ(b.attacks_to(B4, WHITE), 0);

    EXPECT_TRUE(b.attacks_to(bb::rank(RANK1), WHITE));
    EXPECT_TRUE(b.attacks_to(bb::rank(RANK3), WHITE));
    EXPECT_TRUE(b.attacks_to(bb::rank(RANK8), BLACK));
    EXPECT_TRUE(b.attacks_to(bb::rank(RANK6), BLACK));
    EXPECT_FALSE(b.attacks_to(bb::rank(RANK4), WHITE));
    EXPECT_FALSE(b.attacks_to(bb::rank(RANK5), BLACK));
}

TEST(BoardTest, legal_moves_from_pos4b_check) {
    Board b(POS4B);

    EXPECT_TRUE(b.is_check());
    EXPECT_FALSE(b.is_double_check());

    EXPECT_TRUE(b.is_legal_move(Move(G8, H8)));
    EXPECT_TRUE(b.is_legal_move(Move(F8, F7)));
    EXPECT_TRUE(b.is_legal_move(Move(F6, D5)));
    EXPECT_TRUE(b.is_legal_move(Move(D7, D5)));
    EXPECT_TRUE(b.is_legal_move(Move(C5, C4)));
    EXPECT_TRUE(b.is_legal_move(Move(B5, C4)));
    EXPECT_FALSE(b.is_legal_move(Move(G8, F7)));
}

TEST(BoardTest, disable_castle) {
    Board b(STARTFEN);
    b.disable_castle(WHITE);
    EXPECT_FALSE(b.can_castle(WHITE));
    b.disable_castle(BLACK);
    EXPECT_FALSE(b.can_castle(BLACK));
}

TEST(BoardTest, disable_castle_kingside) {
    Board b(STARTFEN);
    b.disable_castle(WHITE, H1);
    EXPECT_TRUE(b.can_castle(WHITE));
    EXPECT_FALSE(b.can_castle_kingside(WHITE));
    EXPECT_TRUE(b.can_castle_queenside(WHITE));
    b.disable_castle(BLACK, H8);
    EXPECT_TRUE(b.can_castle(BLACK));
    EXPECT_FALSE(b.can_castle_kingside(BLACK));
    EXPECT_TRUE(b.can_castle_queenside(BLACK));
}

TEST(BoardTest, disable_castle_queenside) {
    Board b(STARTFEN);
    b.disable_castle(WHITE, A1);
    EXPECT_TRUE(b.can_castle(WHITE));
    EXPECT_TRUE(b.can_castle_kingside(WHITE));
    EXPECT_FALSE(b.can_castle_queenside(WHITE));
    b.disable_castle(BLACK, A8);
    EXPECT_TRUE(b.can_castle(BLACK));
    EXPECT_TRUE(b.can_castle_kingside(BLACK));
    EXPECT_FALSE(b.can_castle_queenside(BLACK));
}

TEST(BoardTest, makeKnightMove) {
    Board b(STARTFEN);
    b.make(Move(G1, F3));
    EXPECT_EQ(b.toFEN(), "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), STARTFEN);
}

TEST(BoardTest, makeBishopCapture) {
    Board b(POS2);
    b.make(Move(E2, A6));
    EXPECT_EQ(b.toFEN(), "r3k2r/p1ppqpb1/Bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), POS2);
}

TEST(BoardTest, makeRookCaptureDisablesCastleRights) {
    auto  fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1 1";
    Board b   = Board(fen);
    b.make(Move(A1, A8));
    EXPECT_EQ(b.toFEN(), "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), fen);
}

TEST(BoardTest, makePawnDoublePushSetsEnpassantSq) {
    auto  fen = "4k3/8/8/8/1p6/8/P7/4K3 w - - 0 1";
    Board b   = Board(fen);
    b.make(Move(A2, A4));
    EXPECT_EQ(b.toFEN(), ENPASSANT_A3);
    b.unmake();
    EXPECT_EQ(b.toFEN(), fen);
}

TEST(BoardTest, makeEnpassantCapture) {
    Board b(ENPASSANT_A3);
    b.make(Move(B4, A3, MOVE_EP));
    EXPECT_EQ(b.toFEN(), "4k3/8/8/8/8/p7/8/4K3 w - - 0 2");
    b.unmake();
    EXPECT_EQ(b.toFEN(), ENPASSANT_A3);
}

TEST(BoardTest, makeCastleKingside) {
    Board b(POS2);
    b.make(Move(E1, G1, MOVE_CASTLE));
    EXPECT_EQ(b.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R4RK1 b kq - 1 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), POS2);
}

TEST(BoardTest, makeCastleQueenside) {
    Board b(POS2);
    b.make(Move(E1, C1, MOVE_CASTLE));
    EXPECT_EQ(b.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2KR3R b kq - 1 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), POS2);
}

TEST(BoardTest, makeKingMoveDisablesCastleRights) {
    Board b(POS2);
    b.make(Move(E1, D1));
    EXPECT_EQ(b.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R2K3R b kq - 1 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), POS2);
}

TEST(BoardTest, makeRookMoveDisablesCastleOORights) {
    Board b(POS2);
    b.make(Move(H1, F1));
    EXPECT_EQ(b.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3KR2 b Qkq - 1 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), POS2);
}

TEST(BoardTest, RookMoveDisablesCastleOOORights) {
    Board b(POS2);
    b.make(Move(A1, C1));
    EXPECT_EQ(b.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 1 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), POS2);
}

TEST(BoardTest, QueenPromotion) {
    auto  fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    Board b   = Board(fen);
    b.make(Move(A7, A8, MOVE_PROM, QUEEN));
    EXPECT_EQ(b.toFEN(), "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), fen);
}

TEST(BoardTest, UnderPromotion) {
    auto  fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    Board b   = Board(fen);
    b.make(Move(A7, A8, MOVE_PROM, BISHOP));
    EXPECT_EQ(b.toFEN(), "B3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), fen);
}
