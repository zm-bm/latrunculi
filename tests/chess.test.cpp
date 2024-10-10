#include "chess.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include "zobrist.hpp"

class ChessTest : public ::testing::Test {
   protected:
    void SetUp() override { Magics::init(); }
};

auto EMPTY_FEN = "4k3/8/8/8/8/8/8/4K3 w - - 0 1",
     PAWN_FEN = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1",
     MOVE_FEN = "4k3/8/8/8/4P3/8/8/4K3 w - - 0 1",
     ENPASSANT_FEN = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1",
     PROMOTION_FEN = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";

TEST_F(ChessTest, MovePieceForward) {
    Chess chess = Chess(PAWN_FEN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2] ^ Zobrist::psq[WHITE][PAWN][E4];
    chess.movePiece<true>(E2, E4, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key);
    EXPECT_EQ(chess.toFEN(), MOVE_FEN);
}

TEST_F(ChessTest, MovePieceBackwards) {
    Chess chess = Chess(PAWN_FEN);
    U64 key = chess.getKey();
    chess.movePiece<false>(E2, E4, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key);
    EXPECT_EQ(chess.toFEN(), MOVE_FEN);
}

TEST_F(ChessTest, AddPieceForward) {
    Chess chess = Chess(EMPTY_FEN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2];
    chess.addPiece<true>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key);
    EXPECT_EQ(chess.toFEN(), PAWN_FEN);
}

TEST_F(ChessTest, AddPieceBackwards) {
    Chess chess = Chess("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    U64 key = chess.getKey();
    chess.addPiece<false>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key);
    EXPECT_EQ(chess.toFEN(), PAWN_FEN);
}

TEST_F(ChessTest, RemovePieceForward) {
    Chess chess = Chess(PAWN_FEN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2];
    chess.removePiece<true>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key);
    // EXPECT_EQ(chess.toFEN(), EMPTY_FEN);
}

TEST_F(ChessTest, RemovePieceBackwards) {
    Chess chess = Chess(PAWN_FEN);
    U64 key = chess.getKey();
    chess.removePiece<false>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key);
    // EXPECT_EQ(chess.toFEN(), EMPTY_FEN);
}

TEST_F(ChessTest, GetCheckingPiecesW) {
    Chess c = Chess(G::POS4W);
    EXPECT_EQ(c.getCheckingPieces(), BB::set(B6));
}

TEST_F(ChessTest, GetCheckingPiecesB) {
    Chess c = Chess(G::POS4B);
    EXPECT_EQ(c.getCheckingPieces(), BB::set(B3));
}

TEST_F(ChessTest, GetEnPassant) {
    Chess c = Chess(ENPASSANT_FEN);
    EXPECT_EQ(c.getEnPassant(), A3);
}

TEST_F(ChessTest, GetHmClock) {
    Chess c = Chess("4k3/8/8/8/8/8/4P3/4K3 w - - 7 1");
    EXPECT_EQ(c.getHmClock(), 7);
}

TEST_F(ChessTest, GetKeyCalculateKey) {
    for (auto fen : G::FENS) {
        Chess c = Chess(fen);
        EXPECT_EQ(c.getKey(), c.calculateKey());
    }
}

TEST_F(ChessTest, IsCheck) {
    EXPECT_FALSE(Chess(G::STARTFEN).getCheckingPieces());
    EXPECT_TRUE(Chess(G::POS4W).getCheckingPieces());
    EXPECT_TRUE(Chess(G::POS4B).getCheckingPieces());
}

TEST_F(ChessTest, IsDoubleCheck) {
    EXPECT_FALSE(Chess(G::POS4W).isDoubleCheck());
    EXPECT_FALSE(Chess(G::POS4B).isDoubleCheck());
    EXPECT_TRUE(Chess("R3k3/8/8/8/8/8/4Q3/4K3 b - - 0 1").isDoubleCheck());
}

TEST_F(ChessTest, CanCastleTrue) {
    Chess c = Chess(G::POS2);
    EXPECT_TRUE(c.canCastle(WHITE));
    EXPECT_TRUE(c.canCastleOO(WHITE));
    EXPECT_TRUE(c.canCastleOOO(WHITE));
    EXPECT_TRUE(c.canCastle(BLACK));
    EXPECT_TRUE(c.canCastleOO(BLACK));
    EXPECT_TRUE(c.canCastleOOO(BLACK));
}

TEST_F(ChessTest, CanCastleFalse) {
    Chess c = Chess(G::POS3);
    EXPECT_FALSE(c.canCastle(WHITE));
    EXPECT_FALSE(c.canCastleOO(WHITE));
    EXPECT_FALSE(c.canCastleOOO(WHITE));
    EXPECT_FALSE(c.canCastle(BLACK));
    EXPECT_FALSE(c.canCastleOO(BLACK));
    EXPECT_FALSE(c.canCastleOOO(BLACK));
}

TEST_F(ChessTest, DisableCastle) {
    Chess c = Chess(G::POS2);
    c.disableCastle(WHITE);
    EXPECT_FALSE(c.canCastle(WHITE));
    c.disableCastle(BLACK);
    EXPECT_FALSE(c.canCastle(BLACK));
}

TEST_F(ChessTest, DisableCastleOO) {
    Chess c = Chess(G::POS2);
    c.disableCastle(WHITE, H1);
    EXPECT_TRUE(c.canCastle(WHITE));
    EXPECT_FALSE(c.canCastleOO(WHITE));
    EXPECT_TRUE(c.canCastleOOO(WHITE));
    c.disableCastle(BLACK, H8);
    EXPECT_TRUE(c.canCastle(BLACK));
    EXPECT_FALSE(c.canCastleOO(BLACK));
    EXPECT_TRUE(c.canCastleOOO(BLACK));
}

TEST_F(ChessTest, DisableCastleOOO) {
    Chess c = Chess(G::POS2);
    c.disableCastle(WHITE, A1);
    EXPECT_TRUE(c.canCastle(WHITE));
    EXPECT_TRUE(c.canCastleOO(WHITE));
    EXPECT_FALSE(c.canCastleOOO(WHITE));
    c.disableCastle(BLACK, A8);
    EXPECT_TRUE(c.canCastle(BLACK));
    EXPECT_TRUE(c.canCastleOO(BLACK));
    EXPECT_FALSE(c.canCastleOOO(BLACK));
}

TEST_F(ChessTest, ChessToFEN) {
    for (auto fen : G::FENS) {
        Chess c = Chess(fen);
        EXPECT_EQ(c.toFEN(), fen);
    }
}

TEST_F(ChessTest, Make) {
    Chess c = Chess(G::STARTFEN);
    c.make(Move(G1, F3));
    EXPECT_EQ(c.toFEN(),
              "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), G::STARTFEN);
}

TEST_F(ChessTest, MakeCapt) {
    Chess c = Chess(G::POS2);
    c.make(Move(E2, A6));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/Bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), G::POS2);
}

TEST_F(ChessTest, MakeCaptRook) {
    auto fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1 1";
    Chess c = Chess(fen);
    c.make(Move(A1, A8));
    EXPECT_EQ(c.toFEN(), "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen);
}

TEST_F(ChessTest, MakeEnpassantSq) {
    auto fen = "4k3/8/8/8/1p6/8/P7/4K3 w - - 0 1";
    Chess c = Chess(fen);
    c.make(Move(A2, A4));
    EXPECT_EQ(c.toFEN(), ENPASSANT_FEN);
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen);
}

TEST_F(ChessTest, MakeEnpassantCapt) {
    Chess c = Chess(ENPASSANT_FEN);
    c.make(Move(B4, A3, ENPASSANT));
    EXPECT_EQ(c.toFEN(),  "4k3/8/8/8/8/p7/8/4K3 w - - 0 2");
    c.unmake();
    EXPECT_EQ(c.toFEN(), ENPASSANT_FEN);
}

TEST_F(ChessTest, MakeCastleOO) {
    Chess c = Chess(G::POS2);
    c.make(Move(E1, G1, CASTLE));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R4RK1 b kq - 1 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), G::POS2);
}

TEST_F(ChessTest, MakeCastleOOO) {
    Chess c = Chess(G::POS2);
    c.make(Move(E1, C1, CASTLE));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2KR3R b kq - 1 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), G::POS2);
}

TEST_F(ChessTest, MakeKingMoveDisableCastleRights) {
    Chess c = Chess(G::POS2);
    c.make(Move(E1, D1));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R2K3R b kq - 1 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), G::POS2);
}

TEST_F(ChessTest, MakeRookMoveDisableCastleOORights) {
    Chess c = Chess(G::POS2);
    c.make(Move(H1, F1));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3KR2 b Qkq - 1 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), G::POS2);
}

TEST_F(ChessTest, MakeRookMoveDisableCastleOOORights) {
    Chess c = Chess(G::POS2);
    c.make(Move(A1, C1));
    EXPECT_EQ(
        c.toFEN(),
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 1 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), G::POS2);
}

TEST_F(ChessTest, MakePromotion) {
    Chess c = Chess(PROMOTION_FEN);
    c.make(Move(A7, A8, PROMOTION, QUEEN));
    EXPECT_EQ(c.toFEN(), "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), PROMOTION_FEN);
}

TEST_F(ChessTest, MakeUnderPromotion) {
    auto fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    Chess c = Chess(fen);
    c.make(Move(A7, A8, PROMOTION, BISHOP));
    EXPECT_EQ(c.toFEN(), "B3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen);
}

TEST_F(ChessTest, IsPseudoLegalMoveLegal) {
    Chess c = Chess(G::POS3);
    EXPECT_TRUE(c.isPseudoLegalMoveLegal(Move(B4, F4)));
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalPinned) {
    Chess c = Chess(G::POS3);
    EXPECT_FALSE(c.isPseudoLegalMoveLegal(Move(B5, B6)));
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalKing) {
    Chess c = Chess(G::POS3);
    EXPECT_FALSE(c.isPseudoLegalMoveLegal(Move(A5, B6)));
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalCastle) {
    Chess c = Chess(G::POS2);
    EXPECT_TRUE(c.isPseudoLegalMoveLegal(Move(E1, G1, CASTLE)));
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalEnPassant) {
    Chess c = Chess(ENPASSANT_FEN);
    EXPECT_TRUE(c.isPseudoLegalMoveLegal(Move(B4, A3, ENPASSANT)));
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalEnPassantPinned) {
    Chess c = Chess("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1");
    EXPECT_FALSE(c.isPseudoLegalMoveLegal(Move(F4, E3, ENPASSANT)));
}

TEST_F(ChessTest, IsCheckingMove) {
    Chess c = Chess("4k3/8/8/8/6N1/8/8/RB1QK3 w - - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(A1, A8)));
    EXPECT_FALSE(c.isCheckingMove(Move(A1, A7)));
    EXPECT_TRUE(c.isCheckingMove(Move(B1, G6)));
    EXPECT_FALSE(c.isCheckingMove(Move(B1, H7)));
    EXPECT_TRUE(c.isCheckingMove(Move(D1, A4)));
    EXPECT_FALSE(c.isCheckingMove(Move(D1, F3)));
    EXPECT_TRUE(c.isCheckingMove(Move(G4, F6)));
    EXPECT_FALSE(c.isCheckingMove(Move(G4, H6)));
}


TEST_F(ChessTest, IsCheckingMoveDiscovered) {
    Chess c = Chess("Q1N1k3/8/2N1N3/8/B7/8/4R3/4K3 w - - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(C8, B6)));
    EXPECT_TRUE(c.isCheckingMove(Move(C6, B8)));
    EXPECT_TRUE(c.isCheckingMove(Move(E6, C5)));
}

TEST_F(ChessTest, IsCheckingMoveDiscoveredEnPassant) {
    Chess c = Chess("4k3/8/8/1pP5/B7/8/8/4K3 w - b6 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(C5, B6, ENPASSANT)));
}

TEST_F(ChessTest, IsCheckingMovePromotion) {
    Chess c = Chess(PROMOTION_FEN);
    EXPECT_TRUE(c.isCheckingMove(Move(A7, A8, PROMOTION, QUEEN)));
    EXPECT_TRUE(c.isCheckingMove(Move(A7, A8, PROMOTION, ROOK)));
    EXPECT_FALSE(c.isCheckingMove(Move(A7, A8, PROMOTION, BISHOP)));
    EXPECT_FALSE(c.isCheckingMove(Move(A7, A8, PROMOTION, KNIGHT)));
}

TEST_F(ChessTest, IsCheckingMoveCastle) {
    Chess c = Chess("5k2/8/8/8/8/8/8/4K2R w K - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(E1, G1, CASTLE)));
}
