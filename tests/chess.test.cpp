#include "chess.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <string>

#include "constants.hpp"
#include "eval.hpp"
#include "zobrist.hpp"

const auto E2PAWN = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";
const auto E4PAWN = "4k3/8/8/8/4P3/8/8/4K3 w - - 0 1";
const auto A3ENPASSANT = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1";

// --- Tests for Chess::eval---
TEST(Chess_eval, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.eval<false>(), TEMPO_BONUS) << "white to move should be tempo bonus";
    c.makeNull();
    EXPECT_EQ(c.eval<false>(), -TEMPO_BONUS) << "black to move should be -tempo";
}
TEST(Chess_eval, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.eval<false>(), TEMPO_BONUS) << "white to move should be tempo bonus";
    c.makeNull();
    EXPECT_EQ(c.eval<false>(), -TEMPO_BONUS) << "black to move should be -tempo";
}
TEST(Chess_eval, WhitePawnOnE2) {
    Chess c(E2PAWN);
    EXPECT_GT(c.eval<false>(), 0) << "white to move should be positive";
    c.makeNull();
    EXPECT_LT(c.eval<false>(), 0) << "black to move should be negative";
}
// --- End tests for Chess::eval---


// TEST(Chess_bishopPawnsScore, Tests) {
// }

// --- Tests for Chess::materialScore ---
TEST(Chess_materialScore, StartPosition) {
    Score score{0, 0};
    EXPECT_EQ(Chess(STARTFEN).materialScore(), score);
}
TEST(Chess_materialScore, WhitePawn) {
    Chess c("4k3/4p3/8/8/8/8/3PP3/4K3 w - - 0 1");
    Score score(Eval::pieceValue(MIDGAME, WHITE, PAWN), Eval::pieceValue(ENDGAME, WHITE, PAWN));
    EXPECT_EQ(c.materialScore(), score);
}
TEST(Chess_materialScore, WhiteKnight) {
    Chess c("4k3/3np3/8/8/8/8/2NNP3/4K3 w - - 0 1");
    Score score(Eval::pieceValue(MIDGAME, WHITE, KNIGHT), Eval::pieceValue(ENDGAME, WHITE, KNIGHT));
    EXPECT_EQ(c.materialScore(), score);
}
TEST(Chess_materialScore, BlackBishop) {
    Chess c("4k3/2bbp3/8/8/8/8/3BP3/4K3 w - - 0 1");
    Score score(Eval::pieceValue(MIDGAME, BLACK, BISHOP), Eval::pieceValue(ENDGAME, BLACK, BISHOP));
    EXPECT_EQ(c.materialScore(), score);
}
TEST(Chess_materialScore, WhiteQueenBlackRook) {
    Chess c("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");
    Score score(Eval::pieceValue(MIDGAME, WHITE, QUEEN) + Eval::pieceValue(MIDGAME, BLACK, ROOK),
                Eval::pieceValue(ENDGAME, WHITE, QUEEN) + Eval::pieceValue(ENDGAME, BLACK, ROOK));
    EXPECT_EQ(c.materialScore(), score);
}
// --- End tests for Chess::materialScore ---

// --- Tests for Chess::psqBonusScore ---
TEST(Chess_psqBonusScore, StartPosition) {
    Score score{0, 0};
    EXPECT_EQ(Chess(STARTFEN).psqBonusScore(), score);
}
TEST(Chess_psqBonusScore, WhiteE2Pawn) {
    Score score(Eval::psqValue(MIDGAME, WHITE, PAWN, E2), Eval::psqValue(ENDGAME, WHITE, PAWN, E2));
    EXPECT_EQ(Chess(E2PAWN).psqBonusScore(), score);
}
// --- End tests for Chess::psqBonusScore ---

TEST(Chess_make, KnightMove) {
    Chess c = Chess(STARTFEN);
    c.make(Move(G1, F3));
    EXPECT_EQ(c.toFEN(), "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1")
        << "should move the knight";
    c.unmake();
    EXPECT_EQ(c.toFEN(), STARTFEN) << "should move the knight back";
}

TEST(Chess_make, BishopCapture) {
    Chess c = Chess(POS2);
    c.make(Move(E2, A6));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/Bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1")
        << "should capture with the bishop";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should undo the capture";
}

TEST(Chess_make, RookCaptureDisablesCastleRights) {
    auto fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1 1";
    Chess c = Chess(fen);
    c.make(Move(A1, A8));
    EXPECT_EQ(c.toFEN(), "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1")
        << "should revoke castle rights with rook capture";
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen) << "should restore castle rights on undo";
}

TEST(Chess_make, PawnDoublePushSetsEnpassantSq) {
    auto fen = "4k3/8/8/8/1p6/8/P7/4K3 w - - 0 1";
    Chess c = Chess(fen);
    c.make(Move(A2, A4));
    EXPECT_EQ(c.toFEN(), A3ENPASSANT) << "should set enpassant square";
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen) << "should clear enpassant square on undo";
}

TEST(Chess_make, EnpassantCapt) {
    Chess c = Chess(A3ENPASSANT);
    c.make(Move(B4, A3, ENPASSANT));
    EXPECT_EQ(c.toFEN(), "4k3/8/8/8/8/p7/8/4K3 w - - 0 2") << "should make enpassant captures";
    c.unmake();
    EXPECT_EQ(c.toFEN(), A3ENPASSANT) << "should undo enpassant captures";
}

TEST(Chess_make, CastleOO) {
    Chess c = Chess(POS2);
    c.make(Move(E1, G1, CASTLE));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R4RK1 b kq - 1 1")
        << "should castle king side";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should undo king side castle";
}

TEST(Chess_make, CastleOOO) {
    Chess c = Chess(POS2);
    c.make(Move(E1, C1, CASTLE));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2KR3R b kq - 1 1")
        << "should castle queen side";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should undo queen side castle";
}

TEST(Chess_make, KingMoveDisablesCastleRights) {
    Chess c = Chess(POS2);
    c.make(Move(E1, D1));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R2K3R b kq - 1 1")
        << "should revoke castle rights with king move";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should restore castle rights on undo";
}

TEST(Chess_make, RookMoveDisablesCastleOORights) {
    Chess c = Chess(POS2);
    c.make(Move(H1, F1));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3KR2 b Qkq - 1 1")
        << "should revoke castle rights on rook move";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should restore rights on undo";
}

TEST(Chess_make, RookMoveDisablesCastleOOORights) {
    Chess c = Chess(POS2);
    c.make(Move(A1, C1));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 1 1")
        << "should revoke castle rights on rook move";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should restore rights on undo";
}

TEST(Chess_make, QueenPromotion) {
    auto fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    Chess c = Chess(fen);
    c.make(Move(A7, A8, PROMOTION, QUEEN));
    EXPECT_EQ(c.toFEN(), "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1") << "should promote to queen";
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen) << "should undo promotion on undo";
}

TEST(Chess_make, UnderPromotion) {
    auto fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    Chess c = Chess(fen);
    c.make(Move(A7, A8, PROMOTION, BISHOP));
    EXPECT_EQ(c.toFEN(), "B3k3/8/8/8/8/8/8/4K3 b - - 0 1") << "should promote to bishop";
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen) << "should undo promotion on undo";
}

TEST(Chess_addPiece, AddPieceForward) {
    Chess chess = Chess(EMPTYFEN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2];
    chess.addPiece<true>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    EXPECT_EQ(chess.toFEN(), E2PAWN) << "should move piece";
}

TEST(Chess_addPiece, AddPieceBackwards) {
    Chess chess = Chess(EMPTYFEN);
    U64 key = chess.getKey();
    chess.addPiece<false>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should not xor key";
    EXPECT_EQ(chess.toFEN(), E2PAWN) << "should move piece";
}

TEST(Chess_removePiece, RemovePieceForwards) {
    Chess chess = Chess(E2PAWN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2];
    chess.removePiece<true>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    // EXPECT_EQ(chess.toFEN(), EMPTYFEN) << "should move piece";
}

TEST(Chess_removePiece, RemovePieceBackwards) {
    Chess chess = Chess(E2PAWN);
    U64 key = chess.getKey();
    chess.removePiece<false>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should not xor key";
    // EXPECT_EQ(chess.toFEN(), EMPTYFEN) << "should move piece";
}

TEST(Chess_movePiece, MovePieceForward) {
    Chess chess = Chess(E2PAWN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2] ^ Zobrist::psq[WHITE][PAWN][E4];
    chess.movePiece<true>(E2, E4, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    EXPECT_EQ(chess.toFEN(), E4PAWN) << "should move piece";
}

TEST(Chess_movePiece, MovePieceBackwards) {
    Chess chess = Chess(E2PAWN);
    U64 key = chess.getKey();
    chess.movePiece<false>(E2, E4, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should not xor key";
    EXPECT_EQ(chess.toFEN(), E4PAWN) << "should move piece";
}

TEST(Chess_getKey_calculateKey, GetKeyEqualsCalculateKey) {
    for (auto fen : FENS) {
        Chess c = Chess(fen);
        EXPECT_EQ(c.getKey(), c.calculateKey()) << "should calculate correct hash key";
    }
}

TEST(Chess_getCheckingPieces, StartPosition) {
    Chess c = Chess(STARTFEN);
    EXPECT_EQ(c.getCheckingPieces(), 0) << "should have no checkers";
}

TEST(Chess_getCheckingPieces, WhiteCheckingPiece) {
    Chess c = Chess(POS4W);
    EXPECT_EQ(c.getCheckingPieces(), BB::set(B6)) << "should have a white checker on b6";
}

TEST(Chess_getCheckingPieces, BlackCheckingPieces) {
    Chess c = Chess(POS4B);
    EXPECT_EQ(c.getCheckingPieces(), BB::set(B3)) << "should have a black checker on b3";
}

TEST(Chess_getEnPassant, StartPosition) {
    Chess c = Chess(STARTFEN);
    EXPECT_EQ(c.getEnPassant(), INVALID) << "should not have valid enpassant square";
}

TEST(Chess_getEnPassant, ValidEnPassantSq) {
    Chess c = Chess(A3ENPASSANT);
    EXPECT_EQ(c.getEnPassant(), A3) << "should have a valid enpassant square";
}

TEST(Chess_getHmClock, GetHmClock) {
    Chess c = Chess("4k3/8/8/8/8/8/4P3/4K3 w - - 7 1");
    EXPECT_EQ(c.getHmClock(), 7) << "should have a half move clock of 7";
}

TEST(Chess_isCheck, CorrectValues) {
    EXPECT_FALSE(Chess(STARTFEN).isCheck()) << "should not be in check";
    EXPECT_TRUE(Chess(POS4W).isCheck()) << "should be in check";
    EXPECT_TRUE(Chess(POS4B).isCheck()) << "should be in check";
}

TEST(Chess_isDoubleCheck, CorrectValues) {
    EXPECT_FALSE(Chess(STARTFEN).isDoubleCheck()) << "should not be in double check";
    EXPECT_FALSE(Chess(POS4W).isDoubleCheck()) << "should not be in double check";
    EXPECT_FALSE(Chess(POS4B).isDoubleCheck()) << "should not be in double check";
    EXPECT_TRUE(Chess("R3k3/8/8/8/8/8/4Q3/4K3 b - - 0 1").isDoubleCheck())
        << "should be in double check";
}

TEST(Chess_isLegalMove, LegalMove) {
    Chess c = Chess(POS3);
    EXPECT_TRUE(c.isLegalMove(Move(B4, F4))) << "should allow legal moves";
}

TEST(Chess_isLegalMove, PinnedMove) {
    Chess c = Chess(POS3);
    EXPECT_FALSE(c.isLegalMove(Move(B5, B6))) << "should not allow moving pins";
}

TEST(Chess_isLegalMove, MovingIntoCheck) {
    Chess c = Chess(POS3);
    EXPECT_FALSE(c.isLegalMove(Move(A5, B6))) << "should not allow moving into check";
}

TEST(Chess_isLegalMove, Castling) {
    Chess c = Chess(POS2);
    EXPECT_TRUE(c.isLegalMove(Move(E1, G1, CASTLE))) << "should allow castles";
}

TEST(Chess_isLegalMove, EnPassant) {
    Chess c = Chess(A3ENPASSANT);
    EXPECT_TRUE(c.isLegalMove(Move(B4, A3, ENPASSANT))) << "should allow legal enpassant";
}

TEST(Chess_isLegalMove, PinnedEnPassant) {
    Chess c = Chess("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1");
    EXPECT_FALSE(c.isLegalMove(Move(F4, E3, ENPASSANT)))
        << "should not allow capturing pinned enpassant";
}

TEST(Chess_isCheckingMove, RegularChecks) {
    Chess c = Chess("4k3/8/8/8/6N1/8/8/RB1QK3 w - - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(A1, A8))) << "should identify rook checks";
    EXPECT_TRUE(c.isCheckingMove(Move(B1, G6))) << "should identify bishop checks";
    EXPECT_TRUE(c.isCheckingMove(Move(D1, A4))) << "should identify queen checks";
    EXPECT_TRUE(c.isCheckingMove(Move(G4, F6))) << "should identify knight checks";
    EXPECT_FALSE(c.isCheckingMove(Move(A1, A7))) << "should identify rook non-checks";
    EXPECT_FALSE(c.isCheckingMove(Move(B1, H7))) << "should identify bishop non-checks";
    EXPECT_FALSE(c.isCheckingMove(Move(D1, F3))) << "should identify queen non-checks";
    EXPECT_FALSE(c.isCheckingMove(Move(G4, H6))) << "should identify knight non-checks";
}

TEST(Chess_isCheckingMove, DiscoveredChecks) {
    Chess c = Chess("Q1N1k3/8/2N1N3/8/B7/8/4R3/4K3 w - - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(C8, B6))) << "should identify queen discovered checks";
    EXPECT_TRUE(c.isCheckingMove(Move(C6, B8))) << "should identify bishop discovered checks";
    EXPECT_TRUE(c.isCheckingMove(Move(E6, C5))) << "should identify rook discovered checks";
}

TEST(Chess_isCheckingMove, DiscoveredEnPassant) {
    Chess c = Chess("4k3/8/8/1pP5/B7/8/8/4K3 w - b6 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(C5, B6, ENPASSANT)))
        << "should identify enpassant discovered check";
}

TEST(Chess_isCheckingMove, Promotions) {
    Chess c = Chess("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(A7, A8, PROMOTION, QUEEN)))
        << "should identify queen prom check";
    EXPECT_TRUE(c.isCheckingMove(Move(A7, A8, PROMOTION, ROOK)))
        << "should identify rook prom check";
    EXPECT_FALSE(c.isCheckingMove(Move(A7, A8, PROMOTION, BISHOP)))
        << "should identify bishop prom non-check";
    EXPECT_FALSE(c.isCheckingMove(Move(A7, A8, PROMOTION, KNIGHT)))
        << "should identify knight prom non-check";
}

TEST(Chess_isCheckingMove, Castles) {
    Chess c = Chess("5k2/8/8/8/8/8/8/4K2R w K - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(E1, G1, CASTLE))) << "should identify castling checks";
}

TEST(Chess_toFen, CorrectValues) {
    for (auto fen : FENS) {
        Chess c = Chess(fen);
        EXPECT_EQ(c.toFEN(), fen) << "should return identical fen";
    }
}
