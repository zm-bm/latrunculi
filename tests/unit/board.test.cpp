#include "board.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "base.hpp"
#include "constants.hpp"
#include "eval.hpp"
#include "score.hpp"
#include "zobrist.hpp"

using enum PieceType;

const auto PAWN_E2        = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";
const auto PAWN_E4        = "4k3/8/8/8/4P3/8/8/4K3 w - - 0 1";
const auto ENPASSANT_A3   = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1";
const std::string FENS[6] = {STARTFEN, POS2, POS3, POS4W, POS4B, POS5};

TEST(BoardTest, piecesEmptyBoard) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.pieces<King>(WHITE), BB::set(E1));
    EXPECT_EQ(b.pieces<King>(BLACK), BB::set(E8));
    EXPECT_EQ(b.pieces<Pawn>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<Pawn>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<Knight>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<Knight>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<Bishop>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<Bishop>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<Rook>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<Rook>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<Queen>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<Queen>(BLACK), 0x0);
}

TEST(BoardTest, pieces_StartBoard) {
    Board b(STARTFEN);
    EXPECT_EQ(b.pieces<King>(WHITE), BB::set(E1));
    EXPECT_EQ(b.pieces<King>(BLACK), BB::set(E8));
    EXPECT_EQ(b.pieces<Pawn>(WHITE), BB::rankBB(Rank::R2));
    EXPECT_EQ(b.pieces<Pawn>(BLACK), BB::rankBB(Rank::R7));
    EXPECT_EQ(b.pieces<Knight>(WHITE), BB::set(B1, G1));
    EXPECT_EQ(b.pieces<Knight>(BLACK), BB::set(B8, G8));
    EXPECT_EQ(b.pieces<Bishop>(WHITE), BB::set(C1, F1));
    EXPECT_EQ(b.pieces<Bishop>(BLACK), BB::set(C8, F8));
    EXPECT_EQ(b.pieces<Rook>(WHITE), BB::set(A1, H1));
    EXPECT_EQ(b.pieces<Rook>(BLACK), BB::set(A8, H8));
    EXPECT_EQ(b.pieces<Queen>(WHITE), BB::set(D1));
    EXPECT_EQ(b.pieces<Queen>(BLACK), BB::set(D8));
}

TEST(BoardTest, occupancyEmptyBoard) {
    U64 expected = BB::set(E1, E8);
    EXPECT_EQ(Board(EMPTYFEN).occupancy(), expected);
}

TEST(BoardTest, occupancyStartBoard) {
    U64 expected =
        BB::rankBB(Rank::R1) | BB::rankBB(Rank::R2) | BB::rankBB(Rank::R7) | BB::rankBB(Rank::R8);
    EXPECT_EQ(Board(STARTFEN).occupancy(), expected);
}

TEST(BoardTest, countEmptyBoard) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.count(WHITE, King), 1);
    EXPECT_EQ(b.count(BLACK, King), 1);
    EXPECT_EQ(b.count(WHITE, Pawn), 0);
    EXPECT_EQ(b.count(BLACK, Pawn), 0);
    EXPECT_EQ(b.count(WHITE, Knight), 0);
    EXPECT_EQ(b.count(BLACK, Knight), 0);
    EXPECT_EQ(b.count(WHITE, Bishop), 0);
    EXPECT_EQ(b.count(BLACK, Bishop), 0);
    EXPECT_EQ(b.count(WHITE, Rook), 0);
    EXPECT_EQ(b.count(BLACK, Rook), 0);
    EXPECT_EQ(b.count(WHITE, Queen), 0);
    EXPECT_EQ(b.count(BLACK, Queen), 0);
}

TEST(BoardTest, countStartBoard) {
    Board b(STARTFEN);
    EXPECT_EQ(b.count(WHITE, King), 1);
    EXPECT_EQ(b.count(BLACK, King), 1);
    EXPECT_EQ(b.count(WHITE, Pawn), 8);
    EXPECT_EQ(b.count(BLACK, Pawn), 8);
    EXPECT_EQ(b.count(WHITE, Knight), 2);
    EXPECT_EQ(b.count(BLACK, Knight), 2);
    EXPECT_EQ(b.count(WHITE, Bishop), 2);
    EXPECT_EQ(b.count(BLACK, Bishop), 2);
    EXPECT_EQ(b.count(WHITE, Rook), 2);
    EXPECT_EQ(b.count(BLACK, Rook), 2);
    EXPECT_EQ(b.count(WHITE, Queen), 1);
    EXPECT_EQ(b.count(BLACK, Queen), 1);
}

TEST(BoardTest, pieceOnEmptyBoard) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.pieceOn(E1), Piece::WKing);
    EXPECT_EQ(b.pieceOn(File::F5, Rank::R1), Piece::WKing);
    EXPECT_EQ(b.pieceOn(E2), Piece::None);
    EXPECT_EQ(b.pieceOn(File::F5, Rank::R2), Piece::None);
}

TEST(BoardTest, pieceOnStartBoard) {
    Board b(STARTFEN);
    EXPECT_EQ(b.pieceOn(A2), Piece::WPawn);
    EXPECT_EQ(b.pieceOn(File::F1, Rank::R2), Piece::WPawn);
    EXPECT_EQ(b.pieceOn(A3), Piece::None);
    EXPECT_EQ(b.pieceOn(File::F1, Rank::R3), Piece::None);
}

TEST(Board_getPieceType, pieceTypeOnEmptyBoard) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.pieceTypeOn(E1), King);
    EXPECT_EQ(b.pieceTypeOn(E2), None);
}

TEST(BoardTest, pieceTypeOnStartBoard) {
    Board b(STARTFEN);
    EXPECT_EQ(b.pieceTypeOn(A2), Pawn);
    EXPECT_EQ(b.pieceTypeOn(A3), None);
}

TEST(BoardTest, kingSqEmptyBoard) {
    Board b(EMPTYFEN);
    EXPECT_EQ(b.kingSq(WHITE), E1);
    EXPECT_EQ(b.kingSq(BLACK), E8);
}

TEST(BoardTest, kingSqStartBoard) {
    Board b(STARTFEN);
    EXPECT_EQ(b.kingSq(WHITE), E1);
    EXPECT_EQ(b.kingSq(BLACK), E8);
}

TEST(BoardTest, sideToMoveWhite) { EXPECT_EQ(Board(STARTFEN).sideToMove(), WHITE); }
TEST(BoardTest, sideToMoveBlack) { EXPECT_EQ(Board(POS4B).sideToMove(), BLACK); }

TEST(BoardTest, materialStartBoard) { EXPECT_EQ(Board(STARTFEN).materialScore(), Score{}); }
TEST(BoardTest, materialWhitePawn) {
    Board b("4k3/4p3/8/8/8/8/3PP3/4K3 w - - 0 1");
    EXPECT_EQ(b.materialScore(), pieceScore(Pawn));
}
TEST(BoardTest, materialBlackBishop) {
    Board b("4k3/2bbp3/8/8/8/8/3BP3/4K3 w - - 0 1");
    EXPECT_EQ(b.materialScore(), pieceScore(Bishop, BLACK));
}
TEST(BoardTest, materialWhiteQueenBlackRook) {
    Board b("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");
    EXPECT_EQ(b.materialScore(), pieceScore(Queen) + pieceScore(Rook, BLACK));
}

TEST(BoardTest, psqBonusStartBoard) { EXPECT_EQ(Board(STARTFEN).psqBonusScore(), Score{}); }
TEST(BoardTest, psqBonusWhiteE2Pawn) {
    EXPECT_EQ(Board(PAWN_E2).psqBonusScore(), pieceSqScore(Pawn, WHITE, E2));
}
TEST(BoardTest, psqBonusBlackD8Queen) {
    Board b("3qk3/8/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_EQ(b.psqBonusScore(), pieceSqScore(Queen, BLACK, D8));
}

TEST(BoardTest, getCastleEmptyBoard) { EXPECT_EQ(Board(EMPTYFEN).castleRights(), NO_CASTLE); }
TEST(BoardTest, getCastlePos5) { EXPECT_EQ(Board(POS5).castleRights(), WHITE_CASTLE); }

TEST(BoardTest, checkersNone) { EXPECT_EQ(Board(STARTFEN).checkers(), 0); }
TEST(BoardTest, checkersWhite) { EXPECT_EQ(Board(POS4W).checkers(), BB::set(B6)); }
TEST(BoardTest, checkersBlack) { EXPECT_EQ(Board(POS4B).checkers(), BB::set(B3)); }

TEST(BoardTest, enPassantInvalid) { EXPECT_EQ(Board(STARTFEN).enPassantSq(), INVALID); }
TEST(BoardTest, enPassantValid) { EXPECT_EQ(Board(ENPASSANT_A3).enPassantSq(), A3); }

TEST(BoardTest, halfmove) { EXPECT_EQ(Board("4k3/8/8/8/8/8/4P3/4K3 w - - 7 1").halfmove(), 7); }

TEST(BoardTest, seeBasicCapture) {
    Board b("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -");
    EXPECT_EQ(b.see(Move(E1, E5)), pieceValue(Pawn));
}
TEST(BoardTest, seeTradingCaptures) {
    Board b("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
    EXPECT_EQ(b.see(Move(D3, E5)), pieceValue(Pawn) - pieceValue(Knight));
}

TEST(BoardTest, isLegalMove) { EXPECT_TRUE(Board(POS3).isLegalMove(Move(B4, F4))); }
TEST(BoardTest, isLegalMovePinnedMove) { EXPECT_FALSE(Board(POS3).isLegalMove(Move(B5, B6))); }
TEST(BoardTest, isLegalMoveMovingIntoCheck) { EXPECT_FALSE(Board(POS3).isLegalMove(Move(A5, B6))); }
TEST(BoardTest, isLegalMoveCastling) {
    EXPECT_TRUE(Board(POS2).isLegalMove(Move(E1, G1, MoveType::Castle)));
}
TEST(BoardTest, isLegalMoveEnPassant) {
    EXPECT_TRUE(Board(ENPASSANT_A3).isLegalMove(Move(B4, A3, MoveType::EnPassant)));
}
TEST(BoardTest, isLegalMovePinnedEnPassant) {
    Board b("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1");
    EXPECT_FALSE(b.isLegalMove(Move(F4, E3, MoveType::EnPassant)));
}

TEST(BoardTest, isCheckingMoveRegularChecks) {
    Board b("4k3/8/8/8/6N1/8/8/RB1QK3 w - - 0 1");
    EXPECT_TRUE(b.isCheckingMove(Move(A1, A8)));
    EXPECT_TRUE(b.isCheckingMove(Move(B1, G6)));
    EXPECT_TRUE(b.isCheckingMove(Move(D1, A4)));
    EXPECT_TRUE(b.isCheckingMove(Move(G4, F6)));
    EXPECT_FALSE(b.isCheckingMove(Move(A1, A7)));
    EXPECT_FALSE(b.isCheckingMove(Move(B1, H7)));
    EXPECT_FALSE(b.isCheckingMove(Move(D1, F3)));
    EXPECT_FALSE(b.isCheckingMove(Move(G4, H6)));
}
TEST(BoardTest, isCheckingMoveDiscoveredChecks) {
    Board b("Q1N1k3/8/2N1N3/8/B7/8/4R3/4K3 w - - 0 1");
    EXPECT_TRUE(b.isCheckingMove(Move(C8, B6)));
    EXPECT_TRUE(b.isCheckingMove(Move(C6, B8)));
    EXPECT_TRUE(b.isCheckingMove(Move(E6, C5)));
}
TEST(BoardTest, isCheckingMoveDiscoveredEnPassant) {
    Board b("4k3/8/8/1pP5/B7/8/8/4K3 w - b6 0 1");
    EXPECT_TRUE(b.isCheckingMove(Move(C5, B6, MoveType::EnPassant)));
}
TEST(BoardTest, isCheckingMovePromotions) {
    Board b("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_TRUE(b.isCheckingMove(Move(A7, A8, MoveType::Promotion, Queen)));
    EXPECT_TRUE(b.isCheckingMove(Move(A7, A8, MoveType::Promotion, Rook)));
    EXPECT_FALSE(b.isCheckingMove(Move(A7, A8, MoveType::Promotion, Bishop)));
    EXPECT_FALSE(b.isCheckingMove(Move(A7, A8, MoveType::Promotion, Knight)));
}
TEST(BoardTEST, isCheckingMoveCastles) {
    Board b("5k2/8/8/8/8/8/8/4K2R w K - 0 1");
    EXPECT_TRUE(b.isCheckingMove(Move(E1, G1, MoveType::Castle)));
}

TEST(BoardTest, isCapture) {
    Board b(POS2);
    EXPECT_TRUE(b.isCapture(Move(D5, E6)));
    EXPECT_TRUE(b.isCapture(Move(F3, F6)));
    EXPECT_FALSE(b.isCapture(Move(A2, A4)));
    EXPECT_FALSE(b.isCapture(Move(C3, B5)));
}

TEST(BoardTest, isCheck) {
    EXPECT_FALSE(Board(STARTFEN).isCheck());
    EXPECT_TRUE(Board(POS4W).isCheck());
    EXPECT_TRUE(Board(POS4B).isCheck());
}

TEST(BoardTest, isDoubleCheck) {
    EXPECT_FALSE(Board(POS4W).isDoubleCheck());
    EXPECT_FALSE(Board(POS4B).isDoubleCheck());
    EXPECT_TRUE(Board("R3k3/8/8/8/8/8/4Q3/4K3 b - - 0 1").isDoubleCheck());
}

TEST(BoardTest, attacksToStartBoard) {
    Board b(STARTFEN);
    EXPECT_EQ(b.attacksTo(A2, WHITE), BB::set(A1));
    EXPECT_EQ(b.attacksTo(A3, WHITE), BB::set(B2, B1));
    EXPECT_EQ(b.attacksTo(A4, WHITE), 0);
    EXPECT_EQ(b.attacksTo(B2, WHITE), BB::set(C1));
    EXPECT_EQ(b.attacksTo(B3, WHITE), BB::set(A2, C2));
    EXPECT_EQ(b.attacksTo(B4, WHITE), 0);
}

TEST(BoardTest, attacksToBBStartBoard) {
    Board b(STARTFEN);
    EXPECT_TRUE(b.attacksTo(BB::rankBB(Rank::R1), WHITE));
    EXPECT_TRUE(b.attacksTo(BB::rankBB(Rank::R3), WHITE));
    EXPECT_TRUE(b.attacksTo(BB::rankBB(Rank::R8), BLACK));
    EXPECT_TRUE(b.attacksTo(BB::rankBB(Rank::R6), BLACK));
    EXPECT_FALSE(b.attacksTo(BB::rankBB(Rank::R4), WHITE));
    EXPECT_FALSE(b.attacksTo(BB::rankBB(Rank::R5), BLACK));
}

TEST(BoardTest, attacksToPinnedPosition) {
    Board b(POS3);
    EXPECT_TRUE(b.attacksTo(BB::fileBB(File::F8), WHITE));
    EXPECT_TRUE(b.attacksTo(BB::fileBB(File::F2), BLACK));
    EXPECT_FALSE(b.attacksTo(BB::fileBB(File::F7), WHITE));
    EXPECT_FALSE(b.attacksTo(BB::fileBB(File::F1), BLACK));
}

TEST(BoardTest, addPiece) {
    Board board = Board(EMPTYFEN);
    U64 key     = board.getKey() ^ Zobrist::hashPiece(WHITE, Pawn, E2);
    board.addPiece<true>(E2, WHITE, Pawn);

    EXPECT_EQ(board.pieceOn(E2), makePiece(WHITE, Pawn));
    EXPECT_EQ(board.pieces<Pawn>(WHITE), BB::set(E2));
    EXPECT_EQ(board.count(WHITE, Pawn), 1);
    EXPECT_EQ(board.occupancy(), BB::set(E8, E2, E1));
    EXPECT_EQ(board.getKey(), key);
    EXPECT_EQ(board.toFEN(), PAWN_E2);
}

TEST(BoardTest, removePiece) {
    Board board = Board(PAWN_E2);
    U64 key     = board.getKey() ^ Zobrist::hashPiece(WHITE, Pawn, E2);
    board.removePiece<true>(E2, WHITE, Pawn);

    EXPECT_EQ(board.pieceOn(E2), Piece::None);
    EXPECT_EQ(board.pieces<Pawn>(WHITE), 0x0);
    EXPECT_EQ(board.count(WHITE, Pawn), 0);
    EXPECT_EQ(board.occupancy(), BB::set(E8, E1));
    EXPECT_EQ(board.getKey(), key);
    EXPECT_EQ(board.toFEN(), EMPTYFEN);
}

TEST(BoardTest, movePiece) {
    Board board  = Board(PAWN_E2);
    U64 key      = board.getKey();
    key         ^= Zobrist::hashPiece(WHITE, Pawn, E2);
    key         ^= Zobrist::hashPiece(WHITE, Pawn, E4);
    board.movePiece<true>(E2, E4, WHITE, Pawn);

    EXPECT_EQ(board.pieceOn(E2), Piece::None);
    EXPECT_EQ(board.pieceOn(E4), makePiece(WHITE, Pawn));
    EXPECT_EQ(board.pieces<Pawn>(WHITE), BB::set(E4));
    EXPECT_EQ(board.count(WHITE, Pawn), 1);
    EXPECT_EQ(board.occupancy(), BB::set(E8, E1, E4));
    EXPECT_EQ(board.getKey(), key);
    EXPECT_EQ(board.toFEN(), PAWN_E4);
}

TEST(BoardTest, canCastleStartBoard) {
    Board b(STARTFEN);
    EXPECT_TRUE(b.canCastle(WHITE));
    EXPECT_TRUE(b.canCastleOO(WHITE));
    EXPECT_TRUE(b.canCastleOOO(WHITE));
    EXPECT_TRUE(b.canCastle(BLACK));
    EXPECT_TRUE(b.canCastleOO(BLACK));
    EXPECT_TRUE(b.canCastleOOO(BLACK));
}

TEST(BoardTest, canCastleEmptyBoard) {
    Board b(EMPTYFEN);
    EXPECT_FALSE(b.canCastle(WHITE));
    EXPECT_FALSE(b.canCastleOO(WHITE));
    EXPECT_FALSE(b.canCastleOOO(WHITE));
    EXPECT_FALSE(b.canCastle(BLACK));
    EXPECT_FALSE(b.canCastleOO(BLACK));
    EXPECT_FALSE(b.canCastleOOO(BLACK));
}

TEST(BoardTest, disableCastle) {
    Board b(STARTFEN);
    b.disableCastle(WHITE);
    EXPECT_FALSE(b.canCastle(WHITE));
    b.disableCastle(BLACK);
    EXPECT_FALSE(b.canCastle(BLACK));
}

TEST(BoardTest, disableCastleKingSide) {
    Board b;
    b.disableCastle(WHITE, H1);
    EXPECT_TRUE(b.canCastle(WHITE));
    EXPECT_FALSE(b.canCastleOO(WHITE));
    EXPECT_TRUE(b.canCastleOOO(WHITE));
    b.disableCastle(BLACK, H8);
    EXPECT_TRUE(b.canCastle(BLACK));
    EXPECT_FALSE(b.canCastleOO(BLACK));
    EXPECT_TRUE(b.canCastleOOO(BLACK));
}

TEST(BoardTest, disableCastleQueenside) {
    Board b;
    b.disableCastle(WHITE, A1);
    EXPECT_TRUE(b.canCastle(WHITE));
    EXPECT_TRUE(b.canCastleOO(WHITE));
    EXPECT_FALSE(b.canCastleOOO(WHITE));
    b.disableCastle(BLACK, A8);
    EXPECT_TRUE(b.canCastle(BLACK));
    EXPECT_TRUE(b.canCastleOO(BLACK));
    EXPECT_FALSE(b.canCastleOOO(BLACK));
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
    auto fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1 1";
    Board b  = Board(fen);
    b.make(Move(A1, A8));
    EXPECT_EQ(b.toFEN(), "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), fen);
}

TEST(BoardTest, makePawnDoublePushSetsEnpassantSq) {
    auto fen = "4k3/8/8/8/1p6/8/P7/4K3 w - - 0 1";
    Board b  = Board(fen);
    b.make(Move(A2, A4));
    EXPECT_EQ(b.toFEN(), ENPASSANT_A3);
    b.unmake();
    EXPECT_EQ(b.toFEN(), fen);
}

TEST(BoardTest, makeEnpassantCapture) {
    Board b(ENPASSANT_A3);
    b.make(Move(B4, A3, MoveType::EnPassant));
    EXPECT_EQ(b.toFEN(), "4k3/8/8/8/8/p7/8/4K3 w - - 0 2");
    b.unmake();
    EXPECT_EQ(b.toFEN(), ENPASSANT_A3);
}

TEST(BoardTest, makeCastleKingside) {
    Board b(POS2);
    b.make(Move(E1, G1, MoveType::Castle));
    EXPECT_EQ(b.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R4RK1 b kq - 1 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), POS2);
}

TEST(BoardTest, makeCastleQueenside) {
    Board b(POS2);
    b.make(Move(E1, C1, MoveType::Castle));
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
    auto fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    Board b  = Board(fen);
    b.make(Move(A7, A8, MoveType::Promotion, Queen));
    EXPECT_EQ(b.toFEN(), "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), fen);
}

TEST(BoardTest, UnderPromotion) {
    auto fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    Board b  = Board(fen);
    b.make(Move(A7, A8, MoveType::Promotion, Bishop));
    EXPECT_EQ(b.toFEN(), "B3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    b.unmake();
    EXPECT_EQ(b.toFEN(), fen);
}

TEST(ChessTest, LoadsAndOutputsCorrectFens) {
    for (auto fen : FENS) {
        EXPECT_EQ(Board(fen).toFEN(), fen) << "should return identical fen";
    }
}

TEST(BoardTest, ZobristKey) {
    for (auto fen : FENS) {
        Board b(fen);
        EXPECT_EQ(b.getKey(), b.calculateKey());
    }
}
