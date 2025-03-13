#include "chess.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "constants.hpp"
#include "eval.hpp"
#include "score.hpp"
#include "zobrist.hpp"

const auto PAWN_E2 = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";
const auto PAWN_E4 = "4k3/8/8/8/4P3/8/8/4K3 w - - 0 1";
const auto ENPASSANT_A3 = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1";
const std::string FENS[6] = {STARTFEN, POS2, POS3, POS4W, POS4B, POS5};

TEST(Chess_getPieces, EmptyPosition) {
    Chess c(EMPTYFEN);

    EXPECT_EQ(c.pieces<KING>(WHITE), BB::set(E1));
    EXPECT_EQ(c.pieces<KING>(BLACK), BB::set(E8));
    EXPECT_EQ(c.pieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(c.pieces<PAWN>(BLACK), 0x0);
    EXPECT_EQ(c.pieces<KNIGHT>(WHITE), 0x0);
    EXPECT_EQ(c.pieces<KNIGHT>(BLACK), 0x0);
    EXPECT_EQ(c.pieces<BISHOP>(WHITE), 0x0);
    EXPECT_EQ(c.pieces<BISHOP>(BLACK), 0x0);
    EXPECT_EQ(c.pieces<ROOK>(WHITE), 0x0);
    EXPECT_EQ(c.pieces<ROOK>(BLACK), 0x0);
    EXPECT_EQ(c.pieces<QUEEN>(WHITE), 0x0);
    EXPECT_EQ(c.pieces<QUEEN>(BLACK), 0x0);
}

TEST(Chess_getPieces, StartPosition) {
    Chess c(STARTFEN);

    EXPECT_EQ(c.pieces<KING>(WHITE), BB::set(E1));
    EXPECT_EQ(c.pieces<KING>(BLACK), BB::set(E8));
    EXPECT_EQ(c.pieces<PAWN>(WHITE), BB::RANK_MASK[RANK2]);
    EXPECT_EQ(c.pieces<PAWN>(BLACK), BB::RANK_MASK[RANK7]);
    EXPECT_EQ(c.pieces<KNIGHT>(WHITE), BB::set(B1) | BB::set(G1));
    EXPECT_EQ(c.pieces<KNIGHT>(BLACK), BB::set(B8) | BB::set(G8));
    EXPECT_EQ(c.pieces<BISHOP>(WHITE), BB::set(C1) | BB::set(F1));
    EXPECT_EQ(c.pieces<BISHOP>(BLACK), BB::set(C8) | BB::set(F8));
    EXPECT_EQ(c.pieces<ROOK>(WHITE), BB::set(A1) | BB::set(H1));
    EXPECT_EQ(c.pieces<ROOK>(BLACK), BB::set(A8) | BB::set(H8));
    EXPECT_EQ(c.pieces<QUEEN>(WHITE), BB::set(D1));
    EXPECT_EQ(c.pieces<QUEEN>(BLACK), BB::set(D8));
}

TEST(Chess_getPiece, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.pieceOn(E1), makePiece(WHITE, KING));
    EXPECT_EQ(c.pieceOn(E2), Piece::NONE);
}

TEST(Chess_getPiece, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.pieceOn(A2), makePiece(WHITE, PAWN));
    EXPECT_EQ(c.pieceOn(A3), Piece::NONE);
}

TEST(Chess_getPieceType, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.pieceTypeOn(E1), KING);
    EXPECT_EQ(c.pieceTypeOn(E2), NO_PIECE_TYPE);
}

TEST(Chess_getPieceType, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.pieceTypeOn(A2), PAWN);
    EXPECT_EQ(c.pieceTypeOn(A3), NO_PIECE_TYPE);
}

TEST(Chess_getKingSq, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.kingSq(WHITE), E1);
    EXPECT_EQ(c.kingSq(BLACK), E8);
}

TEST(Chess_getKingSq, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.kingSq(WHITE), E1);
    EXPECT_EQ(c.kingSq(BLACK), E8);
}

TEST(Chess_occupancy, EmptyPosition) {
    U64 expected = BB::set(E1) | BB::set(E8);
    EXPECT_EQ(Chess(EMPTYFEN).occupancy(), expected);
}

TEST(Chess_occupancy, StartPosition) {
    U64 expected =
        (BB::RANK_MASK[RANK1] | BB::RANK_MASK[RANK2] | BB::RANK_MASK[RANK7] | BB::RANK_MASK[RANK8]);
    EXPECT_EQ(Chess(STARTFEN).occupancy(), expected);
}

TEST(Chess_attacksTo, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.attacksTo(A2, WHITE), 0);
    EXPECT_EQ(c.attacksTo(A3, WHITE), 0);
    EXPECT_EQ(c.attacksTo(A4, WHITE), 0);
    EXPECT_EQ(c.attacksTo(B2, WHITE), 0);
    EXPECT_EQ(c.attacksTo(B3, WHITE), 0);
    EXPECT_EQ(c.attacksTo(B4, WHITE), 0);
}

TEST(Chess_attacksTo, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.attacksTo(A2, WHITE), BB::set(A1));
    EXPECT_EQ(c.attacksTo(A3, WHITE), BB::set(B2) | BB::set(B1));
    EXPECT_EQ(c.attacksTo(A4, WHITE), 0);
    EXPECT_EQ(c.attacksTo(B2, WHITE), BB::set(C1));
    EXPECT_EQ(c.attacksTo(B3, WHITE), BB::set(A2) | BB::set(C2));
    EXPECT_EQ(c.attacksTo(B4, WHITE), 0);
}

TEST(Chess_calculateCheckBlockers, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.calculateCheckBlockers(BLACK, BLACK), 0);
    EXPECT_EQ(c.calculateCheckBlockers(WHITE, WHITE), 0);
}

TEST(Chess_calculateCheckBlockers, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.calculateCheckBlockers(BLACK, BLACK), 0);
    EXPECT_EQ(c.calculateCheckBlockers(WHITE, WHITE), 0);
}

TEST(Chess_calculateCheckBlockers, PinnedPosition) {
    Chess c(POS3);
    EXPECT_EQ(c.calculateCheckBlockers(WHITE, WHITE), BB::set(B5));
    EXPECT_EQ(c.calculateCheckBlockers(BLACK, BLACK), BB::set(F4));
}

TEST(Chess_calculateDiscoveredCheckers, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.calculateDiscoveredCheckers(WHITE), 0);
    EXPECT_EQ(c.calculateDiscoveredCheckers(BLACK), 0);
}

TEST(Chess_calculateDiscoveredCheckers, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.calculateDiscoveredCheckers(WHITE), 0);
    EXPECT_EQ(c.calculateDiscoveredCheckers(BLACK), 0);
}

TEST(Chess_calculateDiscoveredCheckers, DiscoveredChecksPosition) {
    Chess c("8/2p5/3p4/Kp5r/1R3P1k/8/4P1P1/8 w - -");
    EXPECT_EQ(c.calculateDiscoveredCheckers(WHITE), BB::set(F4));
    EXPECT_EQ(c.calculateDiscoveredCheckers(BLACK), BB::set(B5));
}

TEST(Chess_calculatePinnedPieces, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.calculatePinnedPieces(WHITE), 0);
    EXPECT_EQ(c.calculatePinnedPieces(BLACK), 0);
}

TEST(Chess_calculatePinnedPieces, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.calculatePinnedPieces(WHITE), 0);
    EXPECT_EQ(c.calculatePinnedPieces(BLACK), 0);
}

TEST(Chess_calculatePinnedPieces, PinnedPosition) {
    Chess c(POS3);
    EXPECT_EQ(c.calculatePinnedPieces(WHITE), BB::set(B5));
    EXPECT_EQ(c.calculatePinnedPieces(BLACK), BB::set(F4));
}

TEST(Chess_calculateCheckingPieces, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.calculateCheckingPieces(WHITE), 0);
    EXPECT_EQ(c.calculateCheckingPieces(BLACK), 0);
}

TEST(Chess_calculateCheckingPieces, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.calculateCheckingPieces(WHITE), 0);
    EXPECT_EQ(c.calculateCheckingPieces(BLACK), 0);
}

TEST(Chess_calculateCheckingPieces, WhiteCheckPosition) {
    Chess c(POS4W);
    EXPECT_EQ(c.calculateCheckingPieces(WHITE), BB::set(B6));
    EXPECT_EQ(c.calculateCheckingPieces(BLACK), 0);
}

TEST(Chess_calculateCheckingPieces, BlackCheckPosition) {
    Chess c(POS4B);
    EXPECT_EQ(c.calculateCheckingPieces(WHITE), 0);
    EXPECT_EQ(c.calculateCheckingPieces(BLACK), BB::set(B3));
}

TEST(Chess_isBitboardAttacked, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_TRUE(c.isBitboardAttacked(BB::RANK_MASK[RANK1], WHITE));
    EXPECT_FALSE(c.isBitboardAttacked(BB::RANK_MASK[RANK3], WHITE));
    EXPECT_TRUE(c.isBitboardAttacked(BB::RANK_MASK[FILE8], BLACK));
    EXPECT_FALSE(c.isBitboardAttacked(BB::RANK_MASK[RANK6], BLACK));
}

TEST(Chess_isBitboardAttacked, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_TRUE(c.isBitboardAttacked(BB::RANK_MASK[RANK1], WHITE));
    EXPECT_TRUE(c.isBitboardAttacked(BB::RANK_MASK[RANK3], WHITE));
    EXPECT_FALSE(c.isBitboardAttacked(BB::RANK_MASK[RANK4], WHITE));
    EXPECT_TRUE(c.isBitboardAttacked(BB::RANK_MASK[RANK8], BLACK));
    EXPECT_TRUE(c.isBitboardAttacked(BB::RANK_MASK[RANK6], BLACK));
    EXPECT_FALSE(c.isBitboardAttacked(BB::RANK_MASK[RANK5], BLACK));
}

TEST(Chess_isBitboardAttacked, PinnedPosition) {
    Chess c(POS3);
    EXPECT_TRUE(c.isBitboardAttacked(BB::FILE_MASK[FILE8], WHITE));
    EXPECT_FALSE(c.isBitboardAttacked(BB::FILE_MASK[FILE7], WHITE));
    EXPECT_TRUE(c.isBitboardAttacked(BB::FILE_MASK[FILE2], BLACK));
    EXPECT_FALSE(c.isBitboardAttacked(BB::FILE_MASK[FILE1], BLACK));
}

TEST(Chess_count, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.count<KING>(WHITE), 1);
    EXPECT_EQ(c.count<KING>(BLACK), 1);
    EXPECT_EQ(c.count<PAWN>(WHITE), 0);
    EXPECT_EQ(c.count<PAWN>(BLACK), 0);
    EXPECT_EQ(c.count<KNIGHT>(WHITE), 0);
    EXPECT_EQ(c.count<KNIGHT>(BLACK), 0);
    EXPECT_EQ(c.count<BISHOP>(WHITE), 0);
    EXPECT_EQ(c.count<BISHOP>(BLACK), 0);
    EXPECT_EQ(c.count<ROOK>(WHITE), 0);
    EXPECT_EQ(c.count<ROOK>(BLACK), 0);
    EXPECT_EQ(c.count<QUEEN>(WHITE), 0);
    EXPECT_EQ(c.count<QUEEN>(BLACK), 0);
}

TEST(Chess_count, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.count<KING>(WHITE), 1);
    EXPECT_EQ(c.count<KING>(BLACK), 1);
    EXPECT_EQ(c.count<PAWN>(WHITE), 8);
    EXPECT_EQ(c.count<PAWN>(BLACK), 8);
    EXPECT_EQ(c.count<KNIGHT>(WHITE), 2);
    EXPECT_EQ(c.count<KNIGHT>(BLACK), 2);
    EXPECT_EQ(c.count<BISHOP>(WHITE), 2);
    EXPECT_EQ(c.count<BISHOP>(BLACK), 2);
    EXPECT_EQ(c.count<ROOK>(WHITE), 2);
    EXPECT_EQ(c.count<ROOK>(BLACK), 2);
    EXPECT_EQ(c.count<QUEEN>(WHITE), 1);
    EXPECT_EQ(c.count<QUEEN>(BLACK), 1);
}

// --- Tests for Chess::materialScore ---
TEST(Chess_materialScore, StartPosition) {
    Score score{0, 0};
    EXPECT_EQ(Chess(STARTFEN).materialScore(), score);
}
TEST(Chess_materialScore, WhitePawn) {
    Chess c("4k3/4p3/8/8/8/8/3PP3/4K3 w - - 0 1");
    EXPECT_EQ(c.materialScore(), pieceScore(PAWN));
}
TEST(Chess_materialScore, WhiteKnight) {
    Chess c("4k3/3np3/8/8/8/8/2NNP3/4K3 w - - 0 1");
    EXPECT_EQ(c.materialScore(), pieceScore(KNIGHT));
}
TEST(Chess_materialScore, BlackBishop) {
    Chess c("4k3/2bbp3/8/8/8/8/3BP3/4K3 w - - 0 1");
    EXPECT_EQ(c.materialScore(), pieceScore(BISHOP, BLACK));
}
TEST(Chess_materialScore, WhiteQueenBlackRook) {
    Chess c("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");
    EXPECT_EQ(c.materialScore(), pieceScore(QUEEN) + pieceScore(ROOK, BLACK));
}
// --- End tests for Chess::materialScore ---

// --- Tests for Chess::psqBonusScore ---
TEST(Chess_psqBonusScore, StartPosition) {
    Score score{0, 0};
    EXPECT_EQ(Chess(STARTFEN).psqBonusScore(), score);
}
TEST(Chess_psqBonusScore, WhiteE2Pawn) {
    Score score = pieceSqScore(PAWN, WHITE, E2);
    EXPECT_EQ(Chess(PAWN_E2).psqBonusScore(), score);
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
    EXPECT_EQ(c.toFEN(), ENPASSANT_A3) << "should set enpassant square";
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen) << "should clear enpassant square on undo";
}

TEST(Chess_make, EnpassantCapt) {
    Chess c = Chess(ENPASSANT_A3);
    c.make(Move(B4, A3, ENPASSANT));
    EXPECT_EQ(c.toFEN(), "4k3/8/8/8/8/p7/8/4K3 w - - 0 2") << "should make enpassant captures";
    c.unmake();
    EXPECT_EQ(c.toFEN(), ENPASSANT_A3) << "should undo enpassant captures";
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

TEST(Chess_addPiece, AddE2Pawn) {
    Chess chess = Chess(EMPTYFEN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2];
    chess.addPiece<true>(E2, WHITE, PAWN);

    EXPECT_EQ(chess.pieceOn(E2), makePiece(WHITE, PAWN));
    EXPECT_EQ(chess.pieces<PAWN>(WHITE), BB::set(E2));
    EXPECT_EQ(chess.count<PAWN>(WHITE), 1);
    EXPECT_EQ(chess.occupancy(), BB::set(E8) | BB::set(E2) | BB::set(E1));
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    EXPECT_EQ(chess.toFEN(), PAWN_E2) << "should move piece";
}

TEST(Chess_removePiece, RemoveE2Pawn) {
    Chess chess = Chess(PAWN_E2);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2];
    chess.removePiece<true>(E2, WHITE, PAWN);

    // EXPECT_EQ(chess.pieceOn(E2), Piece::NONE);
    EXPECT_EQ(chess.pieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(chess.count<PAWN>(WHITE), 0);
    EXPECT_EQ(chess.occupancy(), BB::set(E8) | BB::set(E1));
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    // EXPECT_EQ(chess.toFEN(), EMPTYFEN) << "should move piece";
}

TEST(Chess_movePiece, MovePieceE2E4) {
    Chess chess = Chess(PAWN_E2);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2] ^ Zobrist::psq[WHITE][PAWN][E4];
    chess.movePiece<true>(E2, E4, WHITE, PAWN);

    EXPECT_EQ(chess.pieceOn(E2), Piece::NONE);
    EXPECT_EQ(chess.pieceOn(E4), makePiece(WHITE, PAWN));
    EXPECT_EQ(chess.pieces<PAWN>(WHITE), BB::set(E4));
    EXPECT_EQ(chess.count<PAWN>(WHITE), 1);
    EXPECT_EQ(chess.occupancy(), BB::set(E8) | BB::set(E1) | BB::set(E4));
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    EXPECT_EQ(chess.toFEN(), PAWN_E4) << "should move piece";
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
    Chess c = Chess(ENPASSANT_A3);
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
    Chess c = Chess(ENPASSANT_A3);
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

TEST(ChessTest, CanCastleTrue) {
    Chess c;
    EXPECT_TRUE(c.canCastle(WHITE));
    EXPECT_TRUE(c.canCastleOO(WHITE));
    EXPECT_TRUE(c.canCastleOOO(WHITE));
    EXPECT_TRUE(c.canCastle(BLACK));
    EXPECT_TRUE(c.canCastleOO(BLACK));
    EXPECT_TRUE(c.canCastleOOO(BLACK));
}

TEST(ChessTest, CanCastleFalse) {
    Chess c(EMPTYFEN);
    EXPECT_FALSE(c.canCastle(WHITE));
    EXPECT_FALSE(c.canCastleOO(WHITE));
    EXPECT_FALSE(c.canCastleOOO(WHITE));
    EXPECT_FALSE(c.canCastle(BLACK));
    EXPECT_FALSE(c.canCastleOO(BLACK));
    EXPECT_FALSE(c.canCastleOOO(BLACK));
}

TEST(ChessTest, DisableCastle) {
    Chess c;
    c.disableCastle(WHITE);
    EXPECT_FALSE(c.canCastle(WHITE));
    c.disableCastle(BLACK);
    EXPECT_FALSE(c.canCastle(BLACK));
}

TEST(ChessTest, DisableCastleOO) {
    Chess c;
    c.disableCastle(WHITE, H1);
    EXPECT_TRUE(c.canCastle(WHITE));
    EXPECT_FALSE(c.canCastleOO(WHITE));
    EXPECT_TRUE(c.canCastleOOO(WHITE));
    c.disableCastle(BLACK, H8);
    EXPECT_TRUE(c.canCastle(BLACK));
    EXPECT_FALSE(c.canCastleOO(BLACK));
    EXPECT_TRUE(c.canCastleOOO(BLACK));
}

TEST(ChessTest, DisableCastleOOO) {
    Chess c;
    c.disableCastle(WHITE, A1);
    EXPECT_TRUE(c.canCastle(WHITE));
    EXPECT_TRUE(c.canCastleOO(WHITE));
    EXPECT_FALSE(c.canCastleOOO(WHITE));
    c.disableCastle(BLACK, A8);
    EXPECT_TRUE(c.canCastle(BLACK));
    EXPECT_TRUE(c.canCastleOO(BLACK));
    EXPECT_FALSE(c.canCastleOOO(BLACK));
}

TEST(ChessTest, LoadsAndOutputsCorrectFens) {
    for (auto fen : FENS) {
        EXPECT_EQ(Chess(fen).toFEN(), fen) << "should return identical fen";
    }
}
