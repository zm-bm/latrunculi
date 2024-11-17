#include "eval.hpp"

#include <gtest/gtest.h>

#include "constants.hpp"
#include "board.hpp"

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
    EXPECT_EQ(Eval::calculatePhase(b.nonPawnMaterial(BLACK)*2), Eval::PHASE_LIMIT);
}

TEST(EvalTest, TempoBonus) {
    EXPECT_EQ(Eval::tempoBonus(WHITE), Eval::TEMPO_BONUS);
    EXPECT_EQ(Eval::tempoBonus(BLACK), -Eval::TEMPO_BONUS);
}
