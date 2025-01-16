#include "chess.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "constants.hpp"
#include "eval.hpp"
#include "zobrist.hpp"

const auto E2PAWN = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";
const auto E4PAWN = "4k3/8/8/8/4P3/8/8/4K3 w - - 0 1";
const auto A3ENPASSANT = "4k3/8/8/8/Pp6/8/8/4K3 b - a3 0 1";

TEST(Chess_passedPawns, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.passedPawns(WHITE), 0);
    EXPECT_EQ(c.passedPawns(BLACK), 0);
}

TEST(Chess_passedPawns, NoPassedPawns) {
    Chess c("4k3/p2p4/8/8/8/8/P1P5/4K3 w - - 0 1");
    EXPECT_EQ(c.passedPawns(WHITE), 0);
    EXPECT_EQ(c.passedPawns(BLACK), 0);
}

TEST(Chess_passedPawns, HasPassedPawns) {
    Chess c("4k3/p3p3/8/8/8/8/P1P5/4K3 w - - 0 1");
    EXPECT_EQ(c.passedPawns(WHITE), BB::set(C2));
    EXPECT_EQ(c.passedPawns(BLACK), BB::set(E7));
}

// TEST(Chess_outpostSquares, Tests) {
// }

TEST(Chess_knightOutposts, NoOutpost) {
    Chess c("6k1/8/8/4p3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(c.knightOutposts(c.outpostSquares<WHITE>(), c.outpostSquares<BLACK>()), 0);
}

TEST(Chess_knightOutposts, BothOutpost) {
    Chess c("6k1/8/8/3Np3/3nP3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(c.knightOutposts(c.outpostSquares<WHITE>(), c.outpostSquares<BLACK>()), 0);
}

TEST(Chess_knightOutposts, WhiteOutpost) {
    Chess c("6k1/8/8/3Np3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(c.knightOutposts(c.outpostSquares<WHITE>(), c.outpostSquares<BLACK>()), 1);
}

TEST(Chess_knightOutposts, BlackOutpost) {
    Chess c("6k1/8/8/4p3/3nP3/8/8/6K1 w - - 1 1");
    EXPECT_EQ(c.knightOutposts(c.outpostSquares<WHITE>(), c.outpostSquares<BLACK>()), -1);
}

TEST(Chess_bishopOutposts, NoOutpost) {
    Chess c("6k1/8/8/4p3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(c.bishopOutposts(c.outpostSquares<WHITE>(), c.outpostSquares<BLACK>()), 0);
}

TEST(Chess_bishopOutposts, BothOutpost) {
    Chess c("6k1/8/8/3Bp3/3bP3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(c.bishopOutposts(c.outpostSquares<WHITE>(), c.outpostSquares<BLACK>()), 0);
}

TEST(Chess_bishopOutposts, WhiteOutpost) {
    Chess c("6k1/8/8/3Bp3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(c.bishopOutposts(c.outpostSquares<WHITE>(), c.outpostSquares<BLACK>()), 1);
}

TEST(Chess_bishopOutposts, BlackOutpost) {
    Chess c("6k1/8/8/4p3/3bP3/8/8/6K1 w - - 1 1");
    EXPECT_EQ(c.bishopOutposts(c.outpostSquares<WHITE>(), c.outpostSquares<BLACK>()), -1);
}

// TEST(Chess_bishopPawnsScore, Tests) {
// }

// TEST(Chess_minorsBehindPawns, NoMinorsBehindPawn) {
// }

// TEST(Chess_minorsBehindPawns, BothMinorsBehindPawn) {
// }

TEST(Chess_minorsBehindPawns, WhiteMinorsBehindPawn) {
    Chess wn("6k1/8/4p3/8/8/4P3/4N3/6K1 w - - 0 1");
    EXPECT_EQ(wn.piecesEval(), Eval::MINOR_BEHIND_PAWN_BONUS);
    Chess wb("6k1/8/4p3/8/8/4P3/4B3/6K1 w - - 0 1");
    EXPECT_EQ(wb.piecesEval(), Eval::MINOR_BEHIND_PAWN_BONUS);
}

TEST(Chess_minorsBehindPawns, BlackMinorsBehindPawn) {
    Chess bn("6k1/4n3/4p3/8/8/4P3/8/6K1 w - - 0 1");
    EXPECT_EQ(bn.piecesEval(), -Eval::MINOR_BEHIND_PAWN_BONUS);
    Chess bb("6k1/4b3/4p3/8/8/4P3/8/6K1 w - - 0 1");
    EXPECT_EQ(bb.piecesEval(), -Eval::MINOR_BEHIND_PAWN_BONUS);
}

TEST(Chess_piecesEval, ReachableKnightOutpost) {
    Chess w("6k1/8/8/4p3/4P3/2N5/8/6K1 w - - 0 1");
    EXPECT_EQ(w.piecesEval(), Eval::REACHABLE_OUTPOST_BONUS);
    Chess b("6k1/8/2n5/4p3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(b.piecesEval(), -Eval::REACHABLE_OUTPOST_BONUS);
}

TEST(Chess_minorsBehindPawns, EmptyPosition) {
    EXPECT_EQ(Chess(EMPTYFEN).minorsBehindPawns(), 0);
}

TEST(Chess_minorsBehindPawns, StartPosition) {
    EXPECT_EQ(Chess(STARTFEN).minorsBehindPawns(), 0);
}

TEST(Chess_minorsBehindPawns, WhiteMinorBehindPawn) {
    EXPECT_EQ(Chess("4k3/8/8/4p3/4P3/4N3/8/4K3 w - - 0 1").minorsBehindPawns(), 1);
}

TEST(Chess_minorsBehindPawns, BlackMinorBehindPawn) {
    EXPECT_EQ(Chess("4k3/8/4b3/4p3/4P3/8/8/4K3 w - - 0 1").minorsBehindPawns(), -1);
}

TEST(Chess_nonPawnMaterial, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(c.nonPawnMaterial(WHITE), 0);
    EXPECT_EQ(c.nonPawnMaterial(BLACK), 0);
}

TEST(Chess_nonPawnMaterial, StartPosition) {
    Chess c(STARTFEN);
    int mat = (2 * Eval::mgPieceValue(KNIGHT) + 2 * Eval::mgPieceValue(BISHOP) +
               2 * Eval::mgPieceValue(ROOK) + Eval::mgPieceValue(QUEEN));
    EXPECT_EQ(c.nonPawnMaterial(WHITE), mat);
    EXPECT_EQ(c.nonPawnMaterial(BLACK), mat);
}

TEST(Chess_hasOppositeBishops, EmptyPosition) {
    EXPECT_FALSE(Chess(EMPTYFEN).hasOppositeBishops());
}

TEST(Chess_hasOppositeBishops, StartPosition) {
    EXPECT_FALSE(Chess(STARTFEN).hasOppositeBishops());
}

TEST(Chess_hasOppositeBishops, NonOppositeBishops) {
    EXPECT_FALSE(Chess("3bk3/8/8/8/8/8/8/2B1K3 w - - 0 1").hasOppositeBishops());
}

TEST(Chess_hasOppositeBishops, HasOppositeBishops) {
    EXPECT_TRUE(Chess("3bk3/8/8/8/8/8/8/3BK3 w - - 0 1").hasOppositeBishops());
}



TEST(Chess_pawnsEval, IsoPawn) {
    Chess c(E2PAWN);
    auto [mg, eg] = c.pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize iso pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize iso pawns";
}

TEST(Chess_pawnsEval, BackwardsPawn) {
    Chess c("4k3/8/1pp5/1P6/P7/8/8/4K3 w - - 0 1");
    auto [mg, eg] = c.pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize backwards pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize backwards pawns";
}

TEST(Chess_pawnsEval, DoubledPawn) {
    Chess c("4k3/8/8/8/P7/P7/8/4K3 w - - 0 1");
    auto [mg, eg] = c.pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize doubled pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize doubled pawns";
}



TEST(Chess_eval, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(c.eval<false>(), TEMPO_BONUS) << "start board eval should equal tempo bonus";
}

TEST(Chess_eval, PositiveEvalWithWhitePawn) {
    Chess c(E2PAWN);
    EXPECT_GT(c.eval<false>(), 0) << "eval with white pawn on e2 should be positive";
}

TEST(Chess_eval, NegativeEvalWithWhitePawnBlackToMove) {
    Chess c(E2PAWN);
    c.makeNull();
    EXPECT_LT(c.eval<false>(), 0) << "eval with white pawn and black to move should be negative";
}

TEST(Chess_materialScore, StartPosition) {
    EXPECT_EQ(Chess(STARTFEN).materialScore().mg, 0);
    EXPECT_EQ(Chess(STARTFEN).materialScore().eg, 0);
}

TEST(Chess_materialScore, WhitePawn) {
    Chess c("4k3/4p3/8/8/8/8/3PP3/4K3 w - - 0 1");
    EXPECT_EQ(c.materialScore().mg, Eval::mgPieceValue(PAWN));
    EXPECT_EQ(c.materialScore().eg, Eval::egPieceValue(PAWN));
}

TEST(Chess_materialScore, WhiteKnight) {
    Chess c("4k3/3np3/8/8/8/8/2NNP3/4K3 w - - 0 1");
    EXPECT_EQ(c.materialScore().mg, Eval::mgPieceValue(KNIGHT));
    EXPECT_EQ(c.materialScore().eg, Eval::egPieceValue(KNIGHT));
}

TEST(Chess_materialScore, BlackBishop) {
    Chess c("4k3/2bbp3/8/8/8/8/3BP3/4K3 w - - 0 1");
    EXPECT_EQ(c.materialScore().mg, -Eval::mgPieceValue(BISHOP));
    EXPECT_EQ(c.materialScore().eg, -Eval::egPieceValue(BISHOP));
}

TEST(Chess_materialScore, WhiteQueenBlackRook) {
    Chess c("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");
    EXPECT_EQ(c.materialScore().mg, Eval::mgPieceValue(QUEEN) - Eval::mgPieceValue(ROOK));
    EXPECT_EQ(c.materialScore().eg, Eval::egPieceValue(QUEEN) - Eval::egPieceValue(ROOK));
}

TEST(Chess_psqBonusScore, CorrectValues) {
    EXPECT_EQ(Chess(STARTFEN).psqBonusScore().mg, 0);
    EXPECT_EQ(Chess(STARTFEN).psqBonusScore().eg, 0);
    EXPECT_EQ(Chess(E2PAWN).psqBonusScore().mg, Eval::psqValue(MIDGAME, WHITE, PAWN, E2));
    EXPECT_EQ(Chess(E2PAWN).psqBonusScore().eg, Eval::psqValue(ENDGAME, WHITE, PAWN, E2));
}

TEST(Chess_scaleFactor, DrawScenarios) {
    EXPECT_EQ(Chess(EMPTYFEN).scaleFactor(), 0);
    EXPECT_EQ(Chess("3bk3/8/8/8/8/8/8/3NK3 w - - 0 1").scaleFactor(), 0);
    EXPECT_EQ(Chess("2nbk3/8/8/8/8/8/8/2RNK3 w - - 0 1").scaleFactor(), 16);
}

TEST(Chess_scaleFactor, OppositeBishopEndings) {
    // Opposite bishops endings
    EXPECT_EQ(Chess("3bk3/4p3/8/8/8/8/4P3/3BK3 w - - 0 1").scaleFactor(), 36);
    EXPECT_EQ(Chess("3bk3/4p3/8/8/8/8/2PPP3/3BK3 w - - 0 1").scaleFactor(), 40);
    EXPECT_EQ(Chess("3bk3/4p3/8/8/8/8/1PPPP3/3BK3 w - - 0 1").scaleFactor(), 44);
}

TEST(Chess_scaleFactor, OneQueenScenarios) {
    // Single queen scenarios
    EXPECT_EQ(Chess("3qk3/8/8/8/8/8/8/4K3 w - - 0 1").scaleFactor(), 36);
    EXPECT_EQ(Chess("3qk3/8/8/8/8/8/8/3BK3 w - - 0 1").scaleFactor(), 40);
    EXPECT_EQ(Chess("3qk3/8/8/8/8/8/8/2BBK3 w - - 0 1").scaleFactor(), 44);
}

TEST(Chess_scaleFactor, Limit) { EXPECT_EQ(Chess(STARTFEN).scaleFactor(), SCALE_LIMIT); }

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
