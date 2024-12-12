#include "chess.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "constants.hpp"
#include "eval.hpp"
#include "zobrist.hpp"

class ChessTest : public ::testing::Test {
   protected:
    void SetUp() override { Magics::init(); }
};

TEST_F(ChessTest, PawnsEvalIsoPawn) {
    Chess c(E2PAWN);
    auto [mg, eg] = c.pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize iso pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize iso pawns";
}

TEST_F(ChessTest, PawnsEvalBackwardsPawn) {
    Chess c("4k3/8/1pp5/1P6/P7/8/8/4K3 w - - 0 1");
    auto [mg, eg] = c.pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize backwards pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize backwards pawns";
}

TEST_F(ChessTest, PawnsEvalDoubledPawn) {
    Chess c("4k3/8/8/8/P7/P7/8/4K3 w - - 0 1");
    auto [mg, eg] = c.pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize doubled pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize doubled pawns";
}

TEST_F(ChessTest, PiecesEval) {

}

TEST_F(ChessTest, EvalStartBoard) {
    Chess c(STARTFEN);
    int mg = c.phaseEval<MIDGAME>(0, 0);
    EXPECT_EQ(c.eval<false>(), mg + TEMPO_BONUS)
        << "start board eval should equal midgame eval + tempo";
}

TEST_F(ChessTest, EvalEmptyBoard) {
    Chess c(EMPTYFEN);
    int eg = c.phaseEval<ENDGAME>(0, 0);
    EXPECT_EQ(c.eval<false>(), eg + TEMPO_BONUS)
        << "empty board eval should equal endgame eval + tempo";
}

TEST_F(ChessTest, EvalBlackToMove) {
    Chess c(POS4B);
    auto [mgPawns, egPawns] = c.pawnsEval();
    auto [mgPieces, egPieces] = c.pawnsEval();
    int score = -c.phaseEval<MIDGAME>(mgPawns, mgPieces);

    EXPECT_EQ(c.eval<false>(), score + TEMPO_BONUS)
        << "black to move should invert eval";
}

TEST_F(ChessTest, MidGameMaterial) {
    EXPECT_EQ(Chess(STARTFEN).materialScore<MIDGAME>(), 0);
    EXPECT_EQ(Chess("4k3/4p3/8/8/8/8/3PP3/4K3 w - - 0 1").materialScore<MIDGAME>(), Eval::mgPieceValue(PAWN));
    EXPECT_EQ(Chess("4k3/3np3/8/8/8/8/2NNP3/4K3 w - - 0 1").materialScore<MIDGAME>(),
              Eval::mgPieceValue(KNIGHT));
    EXPECT_EQ(Chess("4k3/2bbp3/8/8/8/8/3BP3/4K3 w - - 0 1").materialScore<MIDGAME>(),
              -Eval::mgPieceValue(BISHOP));
    EXPECT_EQ(Chess("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1").materialScore<MIDGAME>(),
              Eval::mgPieceValue(QUEEN) - Eval::mgPieceValue(ROOK));
}

TEST_F(ChessTest, EndGameMaterial) {
    EXPECT_EQ(Chess(STARTFEN).materialScore<ENDGAME>(), 0);
    EXPECT_EQ(Chess("4k3/4p3/8/8/8/8/3PP3/4K3 w - - 0 1").materialScore<ENDGAME>(), Eval::egPieceValue(PAWN));
    EXPECT_EQ(Chess("4k3/3np3/8/8/8/8/2NNP3/4K3 w - - 0 1").materialScore<ENDGAME>(),
              Eval::egPieceValue(KNIGHT));
    EXPECT_EQ(Chess("4k3/2bbp3/8/8/8/8/3BP3/4K3 w - - 0 1").materialScore<ENDGAME>(),
              -Eval::egPieceValue(BISHOP));
    EXPECT_EQ(Chess("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1").materialScore<ENDGAME>(),
              Eval::egPieceValue(QUEEN) - Eval::egPieceValue(ROOK));
}

TEST_F(ChessTest, MidGamePieceSqBonus) {
    EXPECT_EQ(Chess(STARTFEN).pieceSqScore<MIDGAME>(), 0);
    EXPECT_EQ(Chess(EMPTYFEN).pieceSqScore<MIDGAME>(), 0);
    EXPECT_EQ(Chess(E2PAWN).pieceSqScore<MIDGAME>(), Eval::pieceSqBonus(MIDGAME, WHITE, PAWN, E2));
}

TEST_F(ChessTest, EndGamePieceSqBonus) {
    EXPECT_EQ(Chess(STARTFEN).pieceSqScore<ENDGAME>(), 0);
    EXPECT_EQ(Chess(EMPTYFEN).pieceSqScore<ENDGAME>(), 0);
    EXPECT_EQ(Chess(E2PAWN).pieceSqScore<ENDGAME>(), Eval::pieceSqBonus(ENDGAME, WHITE, PAWN, E2));
}

TEST_F(ChessTest, ScaleFactor) {
    // Drawish scenarios
    EXPECT_EQ(Chess(EMPTYFEN).scaleFactor(), 0);
    EXPECT_EQ(Chess("3bk3/8/8/8/8/8/8/3NK3 w - - 0 1").scaleFactor(), 0);
    EXPECT_EQ(Chess("2nbk3/8/8/8/8/8/8/2RNK3 w - - 0 1").scaleFactor(), 16);
    // Opposite bishops endings
    EXPECT_EQ(Chess("3bk3/4p3/8/8/8/8/4P3/3BK3 w - - 0 1").scaleFactor(), 36);
    EXPECT_EQ(Chess("3bk3/4p3/8/8/8/8/2PPP3/3BK3 w - - 0 1").scaleFactor(), 40);
    EXPECT_EQ(Chess("3bk3/4p3/8/8/8/8/1PPPP3/3BK3 w - - 0 1").scaleFactor(), 44);
    // Single queen scenarios
    EXPECT_EQ(Chess("3qk3/8/8/8/8/8/8/4K3 w - - 0 1").scaleFactor(), 36);
    EXPECT_EQ(Chess("3qk3/8/8/8/8/8/8/3BK3 w - - 0 1").scaleFactor(), 40);
    EXPECT_EQ(Chess("3qk3/8/8/8/8/8/8/2BBK3 w - - 0 1").scaleFactor(), 44);
    // Default
    EXPECT_EQ(Chess(STARTFEN).scaleFactor(), 64);
}

TEST_F(ChessTest, Make) {
    Chess c = Chess(STARTFEN);
    c.make(Move(G1, F3));
    EXPECT_EQ(c.toFEN(), "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1")
        << "should move the knight";
    c.unmake();
    EXPECT_EQ(c.toFEN(), STARTFEN) << "should move the knight back";
}

TEST_F(ChessTest, MakeCapt) {
    Chess c = Chess(POS2);
    c.make(Move(E2, A6));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/Bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1")
        << "should capture with the bishop";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should undo the capture";
}

TEST_F(ChessTest, MakeCaptRook) {
    auto fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 1 1";
    Chess c = Chess(fen);
    c.make(Move(A1, A8));
    EXPECT_EQ(c.toFEN(), "R3k2r/8/8/8/8/8/8/4K2R b Kk - 0 1")
        << "should revoke castle rights with rook capture";
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen) << "should restore castle rights on undo";
}

TEST_F(ChessTest, MakeEnpassantSq) {
    auto fen = "4k3/8/8/8/1p6/8/P7/4K3 w - - 0 1";
    Chess c = Chess(fen);
    c.make(Move(A2, A4));
    EXPECT_EQ(c.toFEN(), A3ENPASSANT) << "should set enpassant square";
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen) << "should clear enpassant square on undo";
}

TEST_F(ChessTest, MakeEnpassantCapt) {
    Chess c = Chess(A3ENPASSANT);
    c.make(Move(B4, A3, ENPASSANT));
    EXPECT_EQ(c.toFEN(), "4k3/8/8/8/8/p7/8/4K3 w - - 0 2") << "should make enpassant captures";
    c.unmake();
    EXPECT_EQ(c.toFEN(), A3ENPASSANT) << "should undo enpassant captures";
}

TEST_F(ChessTest, MakeCastleOO) {
    Chess c = Chess(POS2);
    c.make(Move(E1, G1, CASTLE));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R4RK1 b kq - 1 1")
        << "should castle king side";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should undo king side castle";
}

TEST_F(ChessTest, MakeCastleOOO) {
    Chess c = Chess(POS2);
    c.make(Move(E1, C1, CASTLE));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2KR3R b kq - 1 1")
        << "should castle queen side";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should undo queen side castle";
}

TEST_F(ChessTest, MakeKingMoveDisableCastleRights) {
    Chess c = Chess(POS2);
    c.make(Move(E1, D1));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R2K3R b kq - 1 1")
        << "should revoke castle rights with king move";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should restore castle rights on undo";
}

TEST_F(ChessTest, MakeRookMoveDisableCastleOORights) {
    Chess c = Chess(POS2);
    c.make(Move(H1, F1));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3KR2 b Qkq - 1 1")
        << "should revoke castle rights on rook move";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should restore rights on undo";
}

TEST_F(ChessTest, MakeRookMoveDisableCastleOOORights) {
    Chess c = Chess(POS2);
    c.make(Move(A1, C1));
    EXPECT_EQ(c.toFEN(), "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/2R1K2R b Kkq - 1 1")
        << "should revoke castle rights on rook move";
    c.unmake();
    EXPECT_EQ(c.toFEN(), POS2) << "should restore rights on undo";
}

TEST_F(ChessTest, MakePromotion) {
    Chess c = Chess(A7PAWN);
    c.make(Move(A7, A8, PROMOTION, QUEEN));
    EXPECT_EQ(c.toFEN(), "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1") << "should promote to queen";
    c.unmake();
    EXPECT_EQ(c.toFEN(), A7PAWN) << "should undo promotion on undo";
}

TEST_F(ChessTest, MakeUnderPromotion) {
    auto fen = "4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    Chess c = Chess(fen);
    c.make(Move(A7, A8, PROMOTION, BISHOP));
    EXPECT_EQ(c.toFEN(), "B3k3/8/8/8/8/8/8/4K3 b - - 0 1") << "should promote to bishop";
    c.unmake();
    EXPECT_EQ(c.toFEN(), fen) << "should undo promotion on undo";
}

TEST_F(ChessTest, AddPieceForward) {
    Chess chess = Chess(EMPTYFEN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2];
    chess.addPiece<true>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    EXPECT_EQ(chess.toFEN(), E2PAWN) << "should move piece";
}

TEST_F(ChessTest, AddPieceBackwards) {
    Chess chess = Chess(EMPTYFEN);
    U64 key = chess.getKey();
    chess.addPiece<false>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should not xor key";
    EXPECT_EQ(chess.toFEN(), E2PAWN) << "should move piece";
}

TEST_F(ChessTest, RemovePieceForward) {
    Chess chess = Chess(E2PAWN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2];
    chess.removePiece<true>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    // EXPECT_EQ(chess.toFEN(), EMPTYFEN) << "should move piece";
}

TEST_F(ChessTest, RemovePieceBackwards) {
    Chess chess = Chess(E2PAWN);
    U64 key = chess.getKey();
    chess.removePiece<false>(E2, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should not xor key";
    // EXPECT_EQ(chess.toFEN(), EMPTYFEN) << "should move piece";
}

TEST_F(ChessTest, MovePieceForward) {
    Chess chess = Chess(E2PAWN);
    U64 key = chess.getKey() ^ Zobrist::psq[WHITE][PAWN][E2] ^ Zobrist::psq[WHITE][PAWN][E4];
    chess.movePiece<true>(E2, E4, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should xor key";
    EXPECT_EQ(chess.toFEN(), E4PAWN) << "should move piece";
}

TEST_F(ChessTest, MovePieceBackwards) {
    Chess chess = Chess(E2PAWN);
    U64 key = chess.getKey();
    chess.movePiece<false>(E2, E4, WHITE, PAWN);
    EXPECT_EQ(chess.getKey(), key) << "should not xor key";
    EXPECT_EQ(chess.toFEN(), E4PAWN) << "should move piece";
}

TEST_F(ChessTest, GetKeyCalculateKey) {
    for (auto fen : FENS) {
        Chess c = Chess(fen);
        EXPECT_EQ(c.getKey(), c.calculateKey()) << "should calculate correct hash key";
    }
}

TEST_F(ChessTest, GetCheckingPiecesW) {
    Chess c = Chess(POS4W);
    EXPECT_EQ(c.getCheckingPieces(), BB::set(B6)) << "should have a white checker on b6";
}

TEST_F(ChessTest, GetCheckingPiecesB) {
    Chess c = Chess(POS4B);
    EXPECT_EQ(c.getCheckingPieces(), BB::set(B3)) << "should have a black checker on b3";
}

TEST_F(ChessTest, GetEnPassant) {
    Chess c = Chess(A3ENPASSANT);
    EXPECT_EQ(c.getEnPassant(), A3) << "should have a valid enpassant square";
}

TEST_F(ChessTest, GetHmClock) {
    Chess c = Chess("4k3/8/8/8/8/8/4P3/4K3 w - - 7 1");
    EXPECT_EQ(c.getHmClock(), 7) << "should have a half move clock of 7";
}

TEST_F(ChessTest, IsCheck) {
    EXPECT_FALSE(Chess(STARTFEN).getCheckingPieces()) << "should not be in check from start pos";
    EXPECT_TRUE(Chess(POS4W).getCheckingPieces()) << "should be in check";
    EXPECT_TRUE(Chess(POS4B).getCheckingPieces()) << "should be in check";
}

TEST_F(ChessTest, IsDoubleCheck) {
    EXPECT_FALSE(Chess(POS4W).isDoubleCheck()) << "should not be in double check";
    EXPECT_FALSE(Chess(POS4B).isDoubleCheck()) << "should not be in double check";
    EXPECT_TRUE(Chess("R3k3/8/8/8/8/8/4Q3/4K3 b - - 0 1").isDoubleCheck())
        << "should be in double check";
}

TEST_F(ChessTest, IsPseudoLegalMoveLegal) {
    Chess c = Chess(POS3);
    EXPECT_TRUE(c.isPseudoLegalMoveLegal(Move(B4, F4))) << "should allow legal moves";
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalPinned) {
    Chess c = Chess(POS3);
    EXPECT_FALSE(c.isPseudoLegalMoveLegal(Move(B5, B6))) << "should not allow moving pins";
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalKing) {
    Chess c = Chess(POS3);
    EXPECT_FALSE(c.isPseudoLegalMoveLegal(Move(A5, B6))) << "should not allow moving into check";
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalCastle) {
    Chess c = Chess(POS2);
    EXPECT_TRUE(c.isPseudoLegalMoveLegal(Move(E1, G1, CASTLE))) << "should allow castles";
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalEnPassant) {
    Chess c = Chess(A3ENPASSANT);
    EXPECT_TRUE(c.isPseudoLegalMoveLegal(Move(B4, A3, ENPASSANT)))
        << "should allow legal enpassant";
}

TEST_F(ChessTest, IsPseudoLegalMoveLegalEnPassantPinned) {
    Chess c = Chess("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1");
    EXPECT_FALSE(c.isPseudoLegalMoveLegal(Move(F4, E3, ENPASSANT)))
        << "should not allow capturing pinned enpassant";
}

TEST_F(ChessTest, IsCheckingMove) {
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

TEST_F(ChessTest, IsCheckingMoveDiscovered) {
    Chess c = Chess("Q1N1k3/8/2N1N3/8/B7/8/4R3/4K3 w - - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(C8, B6))) << "should identify queen double checks";
    EXPECT_TRUE(c.isCheckingMove(Move(C6, B8))) << "should identify bishop double checks";
    EXPECT_TRUE(c.isCheckingMove(Move(E6, C5))) << "should identify rook double checks";
}

TEST_F(ChessTest, IsCheckingMoveDiscoveredEnPassant) {
    Chess c = Chess("4k3/8/8/1pP5/B7/8/8/4K3 w - b6 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(C5, B6, ENPASSANT)))
        << "should identify enpassant discovered check";
}

TEST_F(ChessTest, IsCheckingMovePromotion) {
    Chess c = Chess(A7PAWN);
    EXPECT_TRUE(c.isCheckingMove(Move(A7, A8, PROMOTION, QUEEN)))
        << "should identify queen prom check";
    EXPECT_TRUE(c.isCheckingMove(Move(A7, A8, PROMOTION, ROOK)))
        << "should identify rook prom check";
    EXPECT_FALSE(c.isCheckingMove(Move(A7, A8, PROMOTION, BISHOP)))
        << "should identify bishop prom non-check";
    EXPECT_FALSE(c.isCheckingMove(Move(A7, A8, PROMOTION, KNIGHT)))
        << "should identify knight prom non-check";
}

TEST_F(ChessTest, IsCheckingMoveCastle) {
    Chess c = Chess("5k2/8/8/8/8/8/8/4K2R w K - 0 1");
    EXPECT_TRUE(c.isCheckingMove(Move(E1, G1, CASTLE))) << "should identify castling checks";
}

TEST_F(ChessTest, ChessToFEN) {
    for (auto fen : FENS) {
        Chess c = Chess(fen);
        EXPECT_EQ(c.toFEN(), fen) << "should return identical fen";
    }
}
