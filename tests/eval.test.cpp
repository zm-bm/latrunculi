#include "eval.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "board.hpp"
#include "constants.hpp"

TEST(EvalTest, PieceValue) {
    for (int ph = MIDGAME; ph < N_PHASES; ++ph) {
        for (int pt = PAWN; pt < N_PIECES; ++pt) {
            EXPECT_EQ(Eval::pieceValue(Phase(ph), WHITE, PieceType(pt)),
                      -Eval::pieceValue(Phase(ph), BLACK, PieceType(pt)));
        }
    }
}

TEST(EvalTest, PieceSqBonus) {
    for (int ph = MIDGAME; ph < N_PHASES; ++ph) {
        for (int pt = PAWN; pt < N_PIECES; ++pt) {
            for (int sq = A1; sq < N_SQUARES; ++sq) {
                int bsq = N_SQUARES - 1 - sq;
                EXPECT_EQ(Eval::pieceSqBonus(Phase(ph), WHITE, PieceType(pt), Square(sq)),
                          -Eval::pieceSqBonus(Phase(ph), BLACK, PieceType(pt), Square(bsq)));
            }
        }
    }
}

TEST(EvalTest, TempoBonus) {
    EXPECT_EQ(Eval::tempoBonus(WHITE), Eval::TEMPO_BONUS);
    EXPECT_EQ(Eval::tempoBonus(BLACK), -Eval::TEMPO_BONUS);
}

TEST(EvalTest, TaperScore) {
    EXPECT_EQ(Eval::taperScore(100, 50, Eval::PHASE_LIMIT), 100);
    EXPECT_EQ(Eval::taperScore(100, 50, Eval::PHASE_LIMIT / 2), 75);
    EXPECT_EQ(Eval::taperScore(100, 50, 0), 50);
}

TEST(EvalTest, CalculatePhase) {
    Board b = Board(STARTFEN);
    EXPECT_EQ(Eval::calculatePhase(0), 0);
    EXPECT_EQ(Eval::calculatePhase(Eval::EG_LIMIT), 0);
    EXPECT_EQ(Eval::calculatePhase(b.nonPawnMaterial(BLACK)), 49);
    EXPECT_EQ(Eval::calculatePhase(Eval::MG_LIMIT), Eval::PHASE_LIMIT);
    EXPECT_EQ(Eval::calculatePhase(b.nonPawnMaterial(BLACK) * 2), Eval::PHASE_LIMIT);
}

TEST(EvalTest, IsolatedPawns) {
    Board b(STARTFEN);
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(WHITE)), 0);
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(BLACK)), 0);

    b.removePiece(B2, WHITE, PAWN);
    b.removePiece(F7, BLACK, PAWN);
    b.removePiece(H7, BLACK, PAWN);
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(WHITE)), BB::set(A2));
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(BLACK)), BB::set(G7));

    b = Board("k7/p7/8/P7/8/P7/P7/K7 w KQkq - 0 1");
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(WHITE)),
              BB::set(A2) | BB::set(A3) | BB::set(A5));
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(BLACK)), BB::set(A7));
}

TEST(EvalTest, BackwardsPawns) {
    Board b(STARTFEN);
    U64 res = Eval::backwardsPawns<WHITE, BLACK>(b.getPieces<PAWN>(WHITE), b.getPieces<PAWN>(BLACK));
    EXPECT_EQ(res, 0);
    res = Eval::backwardsPawns<BLACK, WHITE>(b.getPieces<PAWN>(BLACK), b.getPieces<PAWN>(WHITE));
    EXPECT_EQ(res, 0);

    b = Board("4k3/2p5/1p6/1P6/P7/8/8/4K3 w - - 0 1");
    res = Eval::backwardsPawns<WHITE, BLACK>(b.getPieces<PAWN>(WHITE), b.getPieces<PAWN>(BLACK));
    EXPECT_EQ(res, BB::set(A4));
    res = Eval::backwardsPawns<BLACK, WHITE>(b.getPieces<PAWN>(BLACK), b.getPieces<PAWN>(WHITE));
    EXPECT_EQ(res, BB::set(C7));
}

TEST(EvalTest, DoubledPawns) {
    Board b(STARTFEN);
    U64 res = Eval::doubledPawns<WHITE>(b.getPieces<PAWN>(WHITE));
    EXPECT_EQ(res, 0);
    res = Eval::doubledPawns<BLACK>(b.getPieces<PAWN>(BLACK));
    EXPECT_EQ(res, 0);

    b = Board("4k3/8/pp6/p7/P7/8/P7/4K3 w - - 0 1");
    res = Eval::doubledPawns<WHITE>(b.getPieces<PAWN>(WHITE));
    EXPECT_EQ(res, BB::set(A4));
    res = Eval::doubledPawns<BLACK>(b.getPieces<PAWN>(BLACK));
    EXPECT_EQ(res, 0);
}

TEST(EvalTest, OppositeBishops) {
    Board b(EMPTYFEN);
    bool result = Eval::oppositeBishops(b.getPieces<BISHOP>(WHITE), b.getPieces<BISHOP>(BLACK));
    EXPECT_EQ(result, false) << "empty board does not have opposite bishops";

    b = Board(STARTFEN);
    result = Eval::oppositeBishops(b.getPieces<BISHOP>(WHITE), b.getPieces<BISHOP>(BLACK));
    EXPECT_EQ(result, true) << "start board have opposite bishops";

    b = Board("3bk3/8/8/8/8/8/8/2B1K3 w - - 0 1");
    result = Eval::oppositeBishops(b.getPieces<BISHOP>(WHITE), b.getPieces<BISHOP>(BLACK));
    EXPECT_EQ(result, false)
        << "same color bishops do not have opposite bishops";

    b = Board("3bk3/8/8/8/8/8/8/3BK3 w - - 0 1");
    result = Eval::oppositeBishops(b.getPieces<BISHOP>(WHITE), b.getPieces<BISHOP>(BLACK));
    EXPECT_EQ(result, true) << "different color bishops have opposite bishops";
}
       
