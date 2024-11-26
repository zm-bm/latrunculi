#include "eval.hpp"

#include <gtest/gtest.h>

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
    b = Board("k7/p7/8/P7/8/P7/P7/K7 w KQkq - 0 1");
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(WHITE)), 3);
    EXPECT_EQ(Eval::isolatedPawns(b.getPieces<PAWN>(BLACK)), 1);
}
