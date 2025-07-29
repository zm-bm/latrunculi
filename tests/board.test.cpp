#include "board.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "score.hpp"
#include "test_util.hpp"
#include "zobrist.hpp"

TEST(BoardTest, EMPTYFEN) {
    Board b(EMPTYFEN);

    EXPECT_EQ(b.king_sq(WHITE), E1);
    EXPECT_EQ(b.king_sq(BLACK), E8);
    EXPECT_EQ(b.side_to_move(), WHITE);
    EXPECT_EQ(b.pieces<KING>(WHITE), bb::set(E1));
    EXPECT_EQ(b.pieces<KING>(BLACK), bb::set(E8));
    EXPECT_EQ(b.pieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<PAWN>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<KNIGHT>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<KNIGHT>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<BISHOP>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<BISHOP>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<ROOK>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<ROOK>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<QUEEN>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<QUEEN>(BLACK), 0x0);

    EXPECT_EQ(b.occupancy(), bb::set(E1, E8));

    EXPECT_EQ(b.count(WHITE, KING), 1);
    EXPECT_EQ(b.count(BLACK, KING), 1);
    EXPECT_EQ(b.count(WHITE, PAWN), 0);
    EXPECT_EQ(b.count(BLACK, PAWN), 0);
    EXPECT_EQ(b.count(WHITE, KNIGHT), 0);
    EXPECT_EQ(b.count(BLACK, KNIGHT), 0);
    EXPECT_EQ(b.count(WHITE, BISHOP), 0);
    EXPECT_EQ(b.count(BLACK, BISHOP), 0);
    EXPECT_EQ(b.count(WHITE, ROOK), 0);
    EXPECT_EQ(b.count(BLACK, ROOK), 0);
    EXPECT_EQ(b.count(WHITE, QUEEN), 0);
    EXPECT_EQ(b.count(BLACK, QUEEN), 0);

    EXPECT_EQ(b.piece_on(E1), W_KING);
    EXPECT_EQ(b.piece_on(FILE5, RANK1), W_KING);
    EXPECT_EQ(b.piece_on(E2), NO_PIECE);
    EXPECT_EQ(b.piece_on(FILE5, RANK2), NO_PIECE);
    EXPECT_EQ(b.piecetype_on(E1), KING);
    EXPECT_EQ(b.piecetype_on(E2), NO_PIECETYPE);

    EXPECT_EQ(b.material_score(), Score::Zero);
    EXPECT_EQ(b.psq_bonus_score(), Score::Zero);

    EXPECT_EQ(b.castle_rights(), NO_CASTLE);
    EXPECT_FALSE(b.can_castle(WHITE));
    EXPECT_FALSE(b.can_castle(BLACK));
    EXPECT_FALSE(b.can_castle_kingside(WHITE));
    EXPECT_FALSE(b.can_castle_queenside(WHITE));
    EXPECT_FALSE(b.can_castle_kingside(BLACK));
    EXPECT_FALSE(b.can_castle_queenside(BLACK));

    EXPECT_EQ(b.checkers(), 0);
    EXPECT_EQ(b.blockers(WHITE), 0);
    EXPECT_EQ(b.blockers(BLACK), 0);
    EXPECT_EQ(b.enpassant_sq(), INVALID);

    EXPECT_FALSE(b.is_check());
}

TEST(BoardTest, STARTFEN) {
    Board b(STARTFEN);

    EXPECT_EQ(b.king_sq(WHITE), E1);
    EXPECT_EQ(b.king_sq(BLACK), E8);
    EXPECT_EQ(b.side_to_move(), WHITE);
    EXPECT_EQ(b.pieces<KING>(WHITE), bb::set(E1));
    EXPECT_EQ(b.pieces<KING>(BLACK), bb::set(E8));
    EXPECT_EQ(b.pieces<PAWN>(WHITE), bb::rank(RANK2));
    EXPECT_EQ(b.pieces<PAWN>(BLACK), bb::rank(RANK7));
    EXPECT_EQ(b.pieces<KNIGHT>(WHITE), bb::set(B1, G1));
    EXPECT_EQ(b.pieces<KNIGHT>(BLACK), bb::set(B8, G8));
    EXPECT_EQ(b.pieces<BISHOP>(WHITE), bb::set(C1, F1));
    EXPECT_EQ(b.pieces<BISHOP>(BLACK), bb::set(C8, F8));
    EXPECT_EQ(b.pieces<ROOK>(WHITE), bb::set(A1, H1));
    EXPECT_EQ(b.pieces<ROOK>(BLACK), bb::set(A8, H8));
    EXPECT_EQ(b.pieces<QUEEN>(WHITE), bb::set(D1));
    EXPECT_EQ(b.pieces<QUEEN>(BLACK), bb::set(D8));

    EXPECT_EQ(b.occupancy(), bb::rank(RANK1) | bb::rank(RANK2) | bb::rank(RANK7) | bb::rank(RANK8));

    EXPECT_EQ(b.count(WHITE, KING), 1);
    EXPECT_EQ(b.count(BLACK, KING), 1);
    EXPECT_EQ(b.count(WHITE, PAWN), 8);
    EXPECT_EQ(b.count(BLACK, PAWN), 8);
    EXPECT_EQ(b.count(WHITE, KNIGHT), 2);
    EXPECT_EQ(b.count(BLACK, KNIGHT), 2);
    EXPECT_EQ(b.count(WHITE, BISHOP), 2);
    EXPECT_EQ(b.count(BLACK, BISHOP), 2);
    EXPECT_EQ(b.count(WHITE, ROOK), 2);
    EXPECT_EQ(b.count(BLACK, ROOK), 2);
    EXPECT_EQ(b.count(WHITE, QUEEN), 1);
    EXPECT_EQ(b.count(BLACK, QUEEN), 1);

    EXPECT_EQ(b.piece_on(A2), W_PAWN);
    EXPECT_EQ(b.piece_on(FILE1, RANK2), W_PAWN);
    EXPECT_EQ(b.piece_on(A3), NO_PIECE);
    EXPECT_EQ(b.piece_on(FILE1, RANK3), NO_PIECE);
    EXPECT_EQ(b.piecetype_on(A2), PAWN);
    EXPECT_EQ(b.piecetype_on(A3), NO_PIECETYPE);

    EXPECT_EQ(b.material_score(), Score::Zero);
    EXPECT_EQ(b.psq_bonus_score(), Score::Zero);

    EXPECT_EQ(b.castle_rights(), ALL_CASTLE);
    EXPECT_TRUE(b.can_castle(WHITE));
    EXPECT_TRUE(b.can_castle(BLACK));
    EXPECT_TRUE(b.can_castle_kingside(WHITE));
    EXPECT_TRUE(b.can_castle_queenside(WHITE));
    EXPECT_TRUE(b.can_castle_kingside(BLACK));
    EXPECT_TRUE(b.can_castle_queenside(BLACK));

    EXPECT_EQ(b.checkers(), 0);
    EXPECT_EQ(b.blockers(WHITE), 0);
    EXPECT_EQ(b.blockers(BLACK), 0);
    EXPECT_EQ(b.enpassant_sq(), INVALID);

    EXPECT_FALSE(b.is_check());

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

TEST(BoardTest, POS4B) {
    Board b(POS4B);

    EXPECT_EQ(b.king_sq(WHITE), E1);
    EXPECT_EQ(b.king_sq(BLACK), G8);
    EXPECT_EQ(b.side_to_move(), BLACK);
    EXPECT_EQ(b.pieces<KING>(WHITE), bb::set(E1));
    EXPECT_EQ(b.pieces<KING>(BLACK), bb::set(G8));
    EXPECT_EQ(b.pieces<PAWN>(WHITE), bb::set(H2, G2, F2, D2, C2, B2, B7));
    EXPECT_EQ(b.pieces<PAWN>(BLACK), bb::set(H7, G7, E5, D7, C5, B4, A7, A2));
    EXPECT_EQ(b.pieces<KNIGHT>(WHITE), bb::set(A4, F3));
    EXPECT_EQ(b.pieces<KNIGHT>(BLACK), bb::set(F6, H3));
    EXPECT_EQ(b.pieces<BISHOP>(WHITE), bb::set(G3, B3));
    EXPECT_EQ(b.pieces<BISHOP>(BLACK), bb::set(B5, A5));
    EXPECT_EQ(b.pieces<ROOK>(WHITE), bb::set(H1, A1));
    EXPECT_EQ(b.pieces<ROOK>(BLACK), bb::set(F8, A8));
    EXPECT_EQ(b.pieces<QUEEN>(WHITE), bb::set(A6));
    EXPECT_EQ(b.pieces<QUEEN>(BLACK), bb::set(D8));

    EXPECT_EQ(b.count(WHITE, KING), 1);
    EXPECT_EQ(b.count(BLACK, KING), 1);
    EXPECT_EQ(b.count(WHITE, PAWN), 7);
    EXPECT_EQ(b.count(BLACK, PAWN), 8);
    EXPECT_EQ(b.count(WHITE, KNIGHT), 2);
    EXPECT_EQ(b.count(BLACK, KNIGHT), 2);
    EXPECT_EQ(b.count(WHITE, BISHOP), 2);
    EXPECT_EQ(b.count(BLACK, BISHOP), 2);
    EXPECT_EQ(b.count(WHITE, ROOK), 2);
    EXPECT_EQ(b.count(BLACK, ROOK), 2);
    EXPECT_EQ(b.count(WHITE, QUEEN), 1);
    EXPECT_EQ(b.count(BLACK, QUEEN), 1);

    EXPECT_EQ(b.piece_on(B3), W_BISHOP);
    EXPECT_EQ(b.piece_on(FILE6, RANK6), B_KNIGHT);
    EXPECT_EQ(b.piece_on(D4), NO_PIECE);
    EXPECT_EQ(b.piece_on(FILE3, RANK3), NO_PIECE);
    EXPECT_EQ(b.piecetype_on(B7), PAWN);
    EXPECT_EQ(b.piecetype_on(E2), NO_PIECETYPE);

    EXPECT_EQ(b.material_score(), eval::piece(PAWN, BLACK));
    EXPECT_LT(b.psq_bonus_score().mg, 0);

    EXPECT_EQ(b.castle_rights(), W_CASTLE);
    EXPECT_TRUE(b.can_castle(WHITE));
    EXPECT_FALSE(b.can_castle(BLACK));
    EXPECT_TRUE(b.can_castle_kingside(WHITE));
    EXPECT_TRUE(b.can_castle_queenside(WHITE));
    EXPECT_FALSE(b.can_castle_kingside(BLACK));
    EXPECT_FALSE(b.can_castle_queenside(BLACK));

    EXPECT_EQ(b.checkers(), bb::set(B3));
    EXPECT_EQ(b.blockers(WHITE), 0);
    EXPECT_EQ(b.blockers(BLACK), 0);
    EXPECT_EQ(b.enpassant_sq(), INVALID);

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

TEST(BoardTest, material_psq_bonus) {
    Board b("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");
    EXPECT_EQ(b.material_score(), eval::piece(QUEEN, WHITE) + eval::piece(ROOK, BLACK));
    EXPECT_EQ(b.psq_bonus_score(),
              eval::piece_sq(QUEEN, WHITE, D1) + eval::piece_sq(ROOK, BLACK, D8));
}

TEST(BoardTest, enpassant_sq) {
    EXPECT_EQ(Board(ENPASSANT_A3).enpassant_sq(), A3);
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

TEST(BoardTest, is_legal_move_pin) {
    EXPECT_FALSE(Board(POS3).is_legal_move(Move(B5, B6)));
}

TEST(BoardTest, is_legal_move_moving_into_check) {
    EXPECT_FALSE(Board(POS3).is_legal_move(Move(A5, B6)));
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

TEST(BoardTest, add_piece) {
    Board    board = Board(EMPTYFEN);
    uint64_t key   = board.key() ^ zob::hash_piece(WHITE, PAWN, E2);
    board.add_piece<true>(E2, WHITE, PAWN);

    EXPECT_EQ(board.piece_on(E2), make_piece(WHITE, PAWN));
    EXPECT_EQ(board.pieces<PAWN>(WHITE), bb::set(E2));
    EXPECT_EQ(board.count(WHITE, PAWN), 1);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E2, E1));
    EXPECT_EQ(board.key(), key);
    EXPECT_EQ(board.toFEN(), PAWN_E2);
}

TEST(BoardTest, remove_piece) {
    Board    board = Board(PAWN_E2);
    uint64_t key   = board.key() ^ zob::hash_piece(WHITE, PAWN, E2);
    board.remove_piece<true>(E2, WHITE, PAWN);

    EXPECT_EQ(board.piece_on(E2), NO_PIECE);
    EXPECT_EQ(board.pieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(board.count(WHITE, PAWN), 0);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E1));
    EXPECT_EQ(board.key(), key);
    EXPECT_EQ(board.toFEN(), EMPTYFEN);
}

TEST(BoardTest, move_piece) {
    Board    board  = Board(PAWN_E2);
    uint64_t key    = board.key();
    key            ^= zob::hash_piece(WHITE, PAWN, E2);
    key            ^= zob::hash_piece(WHITE, PAWN, E4);
    board.move_piece<true>(E2, E4, WHITE, PAWN);

    EXPECT_EQ(board.piece_on(E2), NO_PIECE);
    EXPECT_EQ(board.piece_on(E4), make_piece(WHITE, PAWN));
    EXPECT_EQ(board.pieces<PAWN>(WHITE), bb::set(E4));
    EXPECT_EQ(board.count(WHITE, PAWN), 1);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E1, E4));
    EXPECT_EQ(board.key(), key);
    EXPECT_EQ(board.toFEN(), PAWN_E4);
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

const std::string test_positions[] = {
    STARTFEN,
    POS2,
    POS3,
    POS4W,
    POS4B,
    POS5,
    POS6,
    EMPTYFEN,
    ENPASSANT_A3,
    E2E4,
};

TEST(BoardTest, LoadsAndOutputsCorrectFens) {
    for (auto fen : test_positions) {
        EXPECT_EQ(Board(fen).toFEN(), fen) << "should return identical fen";
    }
}

TEST(BoardTest, ZobristKey) {
    for (auto fen : test_positions) {
        Board b(fen);
        EXPECT_EQ(b.key(), b.calculate_key());
    }
}

TEST(BoardTest, NonPawnMaterial) {
    int mat = 2 * KNIGHT_MG + 2 * BISHOP_MG + 2 * ROOK_MG + QUEEN_MG;
    std::vector<std::tuple<std::string, Color, int>> test_cases = {
        {EMPTYFEN, WHITE, 0},
        {EMPTYFEN, BLACK, 0},
        {STARTFEN, WHITE, mat},
        {STARTFEN, BLACK, mat},
        {"4k3/8/8/8/8/8/8/4K1NR w K - 0 1", WHITE, KNIGHT_MG + ROOK_MG},
        {"4k1nr/8/8/8/8/8/8/4K3 w k - 0 1", BLACK, KNIGHT_MG + ROOK_MG},
    };

    for (const auto& [fen, color, expected] : test_cases) {
        Board board(fen);
        EXPECT_EQ(board.nonPawnMaterial(color), expected);
    }
}
