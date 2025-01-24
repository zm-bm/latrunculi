#include "evaluator.hpp"

#include <gtest/gtest.h>

#include "chess.hpp"

// --- Tests for Evaluator::eval---
TEST(Evaluator_eval, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(Evaluator(c).eval<false>(), TEMPO_BONUS) << "white to move should be tempo bonus";
    c.makeNull();
    EXPECT_EQ(Evaluator(c).eval<false>(), -TEMPO_BONUS) << "black to move should be -tempo";
}
TEST(Evaluator_eval, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).eval<false>(), TEMPO_BONUS) << "white to move should be tempo bonus";
    c.makeNull();
    EXPECT_EQ(Evaluator(c).eval<false>(), -TEMPO_BONUS) << "black to move should be -tempo";
}
TEST(Evaluator_eval, WhitePawnOnE2) {
    Chess c("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
    EXPECT_GT(Evaluator(c).eval<false>(), 0) << "white to move should be positive";
    c.makeNull();
    EXPECT_LT(Evaluator(c).eval<false>(), 0) << "black to move should be negative";
}
// --- End tests for Evaluator::eval---

// --- Tests for Evaluator::pawnsEval---
TEST(Evaluator_pawnsEval, EmptyPosition) {
    Chess c(EMPTYFEN);
    auto [mg, eg] = Evaluator(c).pawnsEval();
    EXPECT_EQ(mg, 0) << "midgame evaluation should equal 0";
    EXPECT_EQ(eg, 0) << "endgame evaluation should equal 0";
}
TEST(Evaluator_pawnsEval, StartPosition) {
    Chess c(STARTFEN);
    auto [mg, eg] = Evaluator(c).pawnsEval();
    EXPECT_EQ(mg, 0) << "midgame evaluation should equal 0";
    EXPECT_EQ(eg, 0) << "endgame evaluation should equal 0";
}
TEST(Evaluator_pawnsEval, IsoPawn) {
    Chess c("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
    auto [mg, eg] = Evaluator(c).pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize iso pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize iso pawns";
}
TEST(Evaluator_pawnsEval, BackwardsPawn) {
    Chess c("4k3/8/1pp5/1P6/P7/8/8/4K3 w - - 0 1");
    auto [mg, eg] = Evaluator(c).pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize backwards pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize backwards pawns";
}
TEST(Evaluator_pawnsEval, DoubledPawn) {
    Chess c("4k3/8/8/8/P7/P7/8/4K3 w - - 0 1");
    auto [mg, eg] = Evaluator(c).pawnsEval();
    EXPECT_LT(mg, 0) << "midgame evaluation should penalize doubled pawns";
    EXPECT_LT(eg, 0) << "endgame evaluation should penalize doubled pawns";
}
// --- End tests for Evaluator::pawnsEval---

// --- Tests for Evaluator::isolatedPawnsCount ---
TEST(Evaluator_isolatedPawns, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(Evaluator(c).isolatedPawnsCount(), 0);
}
TEST(Evaluator_isolatedPawns, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).isolatedPawnsCount(), 0);
}
TEST(Evaluator_isolatedPawns, WhiteIsoPawn) {
    Chess c("rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 1");
    EXPECT_EQ(Evaluator(c).isolatedPawnsCount(), 1);
}
TEST(Evaluator_isolatedPawns, BlackIsoPawn) {
    Chess c("rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    EXPECT_EQ(Evaluator(c).isolatedPawnsCount(), -1);
}
TEST(Evaluator_isolatedPawns, MultipleIsoPawns) {
    Chess c("k7/p7/8/P7/8/P7/P7/K7 w KQkq - 0 1");
    EXPECT_EQ(Evaluator(c).isolatedPawnsCount(), 2);
}
// --- End tests for Evaluator::isolatedPawnsCount ---

// --- Tests for Evaluator::backwardsPawnsCount ---
TEST(Evaluator_backwardsPawnsCount, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(Evaluator(c).backwardsPawnsCount(), 0);
}
TEST(Evaluator_backwardsPawnsCount, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).backwardsPawnsCount(), 0);
}
TEST(Evaluator_backwardsPawnsCount, WhiteBackwardPawn) {
    Chess c("4k3/3pp3/2p5/2P5/1P6/8/8/4K3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).backwardsPawnsCount(), 1);
}
TEST(Evaluator_backwardsPawnsCount, DoubledPawnsNotBackward) {
    Chess c("4k3/3pp3/2p5/2P5/1P6/1P6/8/4K3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).backwardsPawnsCount(), 1);
}
TEST(Evaluator_backwardsPawnsCount, BlackBackwardPawn) {
    Chess c("4k3/3p4/2p5/2P5/PP6/8/8/4K3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).backwardsPawnsCount(), -1);
}
// --- End tests for Evaluator::backwardsPawnsCount ---

// --- Tests for Evaluator::doubledPawnsCount ---
TEST(Evaluator_doubledPawnsCount, StartPostion) {
    Chess c(STARTFEN);
    EXPECT_EQ(Evaluator(c).doubledPawnsCount(), 0);
}
TEST(Evaluator_doubledPawnsCount, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).doubledPawnsCount(), 0);
}
TEST(Evaluator_doubledPawnsCount, WhiteDoubled) {
    Chess c("4k3/8/pp6/p7/P7/8/P7/4K3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).doubledPawnsCount(), 1);
}
TEST(Evaluator_doubledPawnsCount, BlackDoubled) {
    Chess c("5k2/8/p7/p7/P7/PP6/8/4K3 b - - 0 1");
    EXPECT_EQ(Evaluator(c).doubledPawnsCount(), -1);
}
// --- End tests for Evaluator::doubledPawnsCount ---

// --- End tests for Evaluator::scaleFactor ---
TEST(Evaluator_scaleFactor, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(Evaluator(c).scaleFactor(), SCALE_LIMIT);
}
TEST(Evaluator_scaleFactor, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).scaleFactor(), 0);
}
TEST(Evaluator_scaleFactor, DrawScenario1) {
    Chess c("3bk3/8/8/8/8/8/8/3NK3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).scaleFactor(), 0);
}
TEST(Evaluator_scaleFactor, DrawScenario2) {
    Chess c("2nbk3/8/8/8/8/8/8/2RNK3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).scaleFactor(), 16);
}
TEST(Evaluator_scaleFactor, OppositeBishopEnding1) {
    Chess c("3bk3/4p3/8/8/8/8/4P3/3BK3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).scaleFactor(), 36);
}
TEST(Evaluator_scaleFactor, OppositeBishopEnding2) {
    Chess c("3bk3/4p3/8/8/8/8/2PPP3/3BK3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).scaleFactor(), 40);
}
TEST(Evaluator_scaleFactor, OppositeBishopEnding3) {
    Chess c("3bk3/4p3/8/8/8/8/1PPPP3/3BK3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).scaleFactor(), 44);
}
TEST(Evaluator_scaleFactor, OneQueenScenario1) {
    Chess c("3qk3/8/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).scaleFactor(), 36);
}
TEST(Evaluator_scaleFactor, OneQueenScenario2) {
    Chess c("3qk3/8/8/8/8/8/8/3BK3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).scaleFactor(), 40);
}
TEST(Evaluator_scaleFactor, OneQueenScenario3) {
    Chess c("3qk3/8/8/8/8/8/8/2BBK3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).scaleFactor(), 44);
}
// --- End tests for Evaluator::scaleFactor ---

// --- Tests for Evaluator::oppositeBishops
TEST(Evaluator_oppositeBishops, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_FALSE(Evaluator(c).oppositeBishops());
}
TEST(Evaluator_oppositeBishops, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_FALSE(Evaluator(c).oppositeBishops());
}
TEST(Evaluator_oppositeBishops, NonOppositeBishops) {
    Chess c("3bk3/8/8/8/8/8/8/2B1K3 w - - 0 1");
    EXPECT_FALSE(Evaluator(c).oppositeBishops());
}
TEST(Evaluator_oppositeBishops, HasOppositeBishops) {
    Chess c("3bk3/8/8/8/8/8/8/3BK3 w - - 0 1");
    EXPECT_TRUE(Evaluator(c).oppositeBishops());
}
// --- End tests for Evaluator::oppositeBishops

// --- Tests for Evaluator::phase---
TEST(Evaluator_phase, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(Evaluator(c).phase(), PHASE_LIMIT);
}
TEST(Evaluator_phase, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).phase(), 0);
}
TEST(Evaluator_phase, CorrectInterpolation) {
    Chess c("4k3/8/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    EXPECT_EQ(Evaluator(c).phase(), 49);
}
// --- End tests for Evaluator::phase---

// --- Tests for Evaluator::nonPawnMaterial---
TEST(Evaluator_nonPawnMaterial, StartPosition) {
    Chess c(STARTFEN);
    int mat = (2 * Eval::pieceValue(MIDGAME, WHITE, KNIGHT)) +
              (2 * Eval::pieceValue(MIDGAME, WHITE, BISHOP)) +
              (2 * Eval::pieceValue(MIDGAME, WHITE, ROOK)) +
              Eval::pieceValue(MIDGAME, WHITE, QUEEN);
    EXPECT_EQ(Evaluator(c).nonPawnMaterial(WHITE), mat);
    EXPECT_EQ(Evaluator(c).nonPawnMaterial(BLACK), mat);
}
TEST(Evaluator_nonPawnMaterial, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).nonPawnMaterial(WHITE), 0);
    EXPECT_EQ(Evaluator(c).nonPawnMaterial(BLACK), 0);
}
// --- End tests for Evaluator::nonPawnMaterial---
