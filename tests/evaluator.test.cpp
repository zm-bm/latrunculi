#include "evaluator.hpp"

#include <gtest/gtest.h>

#include "chess.hpp"
#include "score.hpp"

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

class EvaluatorTest : public ::testing::Test {
  protected:
    void testScaleFactor(const std::string& fen, int expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.scaleFactor(), expected) << fen;
    }

    void testPawnsEval(const std::string& fen, Score expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.pawnsEval(), expected) << fen;
    }

    void testPiecesEval(const std::string& fen, Score expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.piecesEval(), expected) << fen;
    }

    void testKnightEval(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.knightEval<WHITE>(), expectedWhite) << fen;
        EXPECT_EQ(evaluator.knightEval<BLACK>(), expectedBlack) << fen;
    }

    void testBishopEval(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.bishopEval<WHITE>(), expectedWhite) << fen;
        EXPECT_EQ(evaluator.bishopEval<BLACK>(), expectedBlack) << fen;
    }

    void testQueenEval(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.queenEval<WHITE>(), expectedWhite) << fen;
        EXPECT_EQ(evaluator.queenEval<BLACK>(), expectedBlack) << fen;
    }
};

TEST_F(EvaluatorTest, KnightEval) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, Score{0, 0}, Score{0, 0}},
        {EMPTYFEN, Score{0, 0}, Score{0, 0}},
        {"6k1/8/2p1n3/4p3/4P3/2P1N3/8/6K1 w - - 0 1", REACHABLE_OUTPOST_BONUS, REACHABLE_OUTPOST_BONUS}
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testKnightEval(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvaluatorTest, BishopEval) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, Score{0, 0}, Score{0, 0}},
        {EMPTYFEN, Score{0, 0}, Score{0, 0}},
        {"4k3/ppp3p1/5n2/2b1pb2/8/1PP5/1B3PB1/4K3 w - - 0 1", BISHOP_PAWN_XRAY_PENALTY * 3, BISHOP_PAWN_XRAY_PENALTY}
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testBishopEval(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvaluatorTest, QueenEval) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, Score{0, 0}, Score{0, 0}},
        {EMPTYFEN, Score{0, 0}, Score{0, 0}},
        {"q3k3/r7/8/8/8/R7/R7/Q3K3 w - - 0 1", ROOK_ON_QUEEN_FILE_BONUS * 2, ROOK_ON_QUEEN_FILE_BONUS},
        {"qr2k3/r7/8/8/8/R7/Q7/QR2K3 w - - 0 1", ROOK_ON_QUEEN_FILE_BONUS, ROOK_ON_QUEEN_FILE_BONUS},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testQueenEval(fen, expectedWhite, expectedBlack);
    }
}

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

// --- Tests for Evaluator::knightOutpostCount ---
TEST(Chess_knightOutpostCount, NoOutpost) {
    Chess c("6k1/8/8/4p3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(Evaluator(c).knightOutpostCount(), 0);
}
TEST(Evaluator_knightOutpostCount, BothOutpost) {
    Chess c("6k1/8/8/3Np3/3nP3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(Evaluator(c).knightOutpostCount(), 0);
}
TEST(Evaluator_knightOutpostCount, WhiteOutpost) {
    Chess c("6k1/8/8/3Np3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(Evaluator(c).knightOutpostCount(), 1);
}
TEST(Evaluator_knightOutpostCount, BlackOutpost) {
    Chess c("6k1/8/8/4p3/3nP3/8/8/6K1 w - - 1 1");
    EXPECT_EQ(Evaluator(c).knightOutpostCount(), -1);
}
// --- Tests for Evaluator::knightOutpostCount ---

// --- Tests for Evaluator::bishopOutpostCount ---
TEST(Evaluator_bishopOutpostCount, NoOutpost) {
    Chess c("6k1/8/8/4p3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(Evaluator(c).bishopOutpostCount(), 0);
}
TEST(Evaluator_bishopOutpostCount, BothOutpost) {
    Chess c("6k1/8/8/3Bp3/3bP3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(Evaluator(c).bishopOutpostCount(), 0);
}
TEST(Evaluator_bishopOutpostCount, WhiteOutpost) {
    Chess c("6k1/8/8/3Bp3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(Evaluator(c).bishopOutpostCount(), 1);
}
TEST(Evaluator_bishopOutpostCount, BlackOutpost) {
    Chess c("6k1/8/8/4p3/3bP3/8/8/6K1 w - - 1 1");
    EXPECT_EQ(Evaluator(c).bishopOutpostCount(), -1);
}
// --- Tests for Evaluator::bishopOutpostCount ---

// --- Tests for Evaluator::minorsBehindPawns ---
TEST(Evaluator_minorsBehindPawns, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).minorsBehindPawns(), 0);
}
TEST(Evaluator_minorsBehindPawns, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(Evaluator(c).minorsBehindPawns(), 0);
}
TEST(Evaluator_minorsBehindPawns, WhiteMinorBehindPawn) {
    Chess c("4k3/8/8/4p3/4P3/4N3/8/4K3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).minorsBehindPawns(), 1);
}
TEST(Evaluator_minorsBehindPawns, BlackMinorBehindPawn) {
    Chess c("4k3/8/4b3/4p3/4P3/8/8/4K3 w - - 0 1");
    EXPECT_EQ(Evaluator(c).minorsBehindPawns(), -1);
}
// --- End tests for Evaluator::minorsBehindPawns ---

// --- Tests for Evaluator:bishopPawnBlockers ---
TEST(Evaluator_bishopPawnBlockers, StartPosition) {
    Chess c(STARTFEN);
    EXPECT_EQ(Evaluator(c).bishopPawnBlockers(), 0);
}
TEST(Evaluator_bishopPawnBlockers, EmptyPosition) {
    Chess c(EMPTYFEN);
    EXPECT_EQ(Evaluator(c).bishopPawnBlockers(), 0);
}
TEST(Evaluator_bishopPawnBlockers, MixedWithoutBlockers) {
    Chess c("rn1qkbnr/ppp1pppp/3p4/8/8/4P3/PPPP1PPP/RN1QKBNR w KQkq - 0 1");
    EXPECT_EQ(Evaluator(c).bishopPawnBlockers(), -2);
}
TEST(Evaluator_bishopPawnBlockers, MixedWithBlockers) {
    Chess c("4kb2/5p1p/pp2p1p1/2pp4/2PP4/1P2PP1P/P5P1/4KB2 w - - 0 1");
    EXPECT_EQ(Evaluator(c).bishopPawnBlockers(), 12);
}
TEST(Evaluator_bishopPawnBlockers, DefendedWithBlocker) {
    Chess c("6k1/8/8/3Bp3/4P3/8/8/6K1 w - - 0 1");
    EXPECT_EQ(Evaluator(c).bishopPawnBlockers(), 1);
}
// --- End tests for Evaluator::bishopPawnBlockers ---

// --- Tests for Evaluator:outpostSquares ---
TEST(Evaluator_outpostSquares, StartPosition) {
    Chess c(STARTFEN);
    Evaluator ev(c);
    EXPECT_EQ(ev.outpostSquares<WHITE>(), 0);
    EXPECT_EQ(ev.outpostSquares<BLACK>(), 0);
}
TEST(Evaluator_outpostSquares, EmptyPosition) {
    Chess c(EMPTYFEN);
    Evaluator ev(c);
    EXPECT_EQ(ev.outpostSquares<WHITE>(), 0);
    EXPECT_EQ(ev.outpostSquares<BLACK>(), 0);
}
TEST(Evaluator_outpostSquares, WhiteOutpostOnD5) {
    Chess c("r4rk1/pp3ppp/3p2n1/2p5/4P3/2N5/PPP2PPP/2KRR3 w - - 0 1");
    Evaluator ev(c);
    EXPECT_EQ(ev.outpostSquares<WHITE>(), BB::set(D5));
    EXPECT_EQ(ev.outpostSquares<BLACK>(), 0);
}
TEST(Evaluator_outpostSquares, BlackOutpostOnD4) {
    Chess c("r4rk1/pp2pppp/3pn3/2p5/2P1P3/1N6/PP3PPP/2KRR3 w - - 0 1");
    Evaluator ev(c);
    EXPECT_EQ(ev.outpostSquares<WHITE>(), 0);
    EXPECT_EQ(ev.outpostSquares<BLACK>(), BB::set(D4));
}
TEST(Evaluator_outpostSquares, NoOutpostOn7thRank) {
    Chess c("r4rk1/1p2pppp/1P1pn3/2p5/8/pNPPP3/P4PPP/2KRR3 w - - 0 1");
    Evaluator ev(c);
    EXPECT_EQ(ev.outpostSquares<WHITE>(), 0);
    EXPECT_EQ(ev.outpostSquares<BLACK>(), 0);
}
// --- End tests for Evaluator::outpostSquares ---

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

TEST_F(EvaluatorTest, ScaleFactor) {
    std::vector<std::pair<std::string, int>> testCases = {
        {STARTFEN, SCALE_LIMIT},
        {EMPTYFEN, 0},
        {"3bk3/8/8/8/8/8/8/3NK3 w - - 0 1", 0},
        {"2nbk3/8/8/8/8/8/8/2RNK3 w - - 0 1", 16},
        {"3bk3/4p3/8/8/8/8/4P3/3BK3 w - - 0 1", 36},
        {"3bk3/4p3/8/8/8/8/2PPP3/3BK3 w - - 0 1", 40},
        {"3bk3/4p3/8/8/8/8/1PPPP3/3BK3 w - - 0 1", 44},
        {"3qk3/8/8/8/8/8/8/4K3 w - - 0 1", 36},
        {"3qk3/8/8/8/8/8/8/3BK3 w - - 0 1", 40},
        {"3qk3/8/8/8/8/8/8/2BBK3 w - - 0 1", 44},
    };

    for (const auto& [fen, expected] : testCases) {
        testScaleFactor(fen, expected);
    }
}

TEST_F(EvaluatorTest, PawnsEval) {
    std::vector<std::pair<std::string, Score>> testCases = {
        {STARTFEN, Score{0, 0}},
        {EMPTYFEN, Score{0, 0}},
        {"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", ISO_PAWN_PENALTY},
        {"4k3/8/1pp5/1P6/P7/8/8/4K3 w - - 0 1", BACKWARD_PAWN_PENALTY},
        {"4k3/8/8/8/PP6/P7/8/4K3 w - - 0 1", DOUBLED_PAWN_PENALTY},
    };

    for (const auto& [fen, expected] : testCases) {
        testPawnsEval(fen, expected);
    }
}

TEST_F(EvaluatorTest, PiecesEval) {
    std::vector<std::pair<std::string, Score>> testCases = {
        {STARTFEN, Score{0, 0}},
        {EMPTYFEN, Score{0, 0}},
        {"6k1/8/8/3Np3/4P3/8/8/6K1 w - - 0 1", KNIGHT_OUTPOST_BONUS},
        {"6k1/8/6n1/4p3/4P3/2P5/8/6K1 w - - 0 1", -REACHABLE_OUTPOST_BONUS},
        {"6k1/8/8/3Bp3/4P3/8/8/6K1 w - - 0 1", BISHOP_OUTPOST_BONUS + BISHOP_PAWN_BLOCKER_PENALTY},
        {"6k1/8/4p3/8/8/4P3/4N3/6K1 w - - 0 1", MINOR_BEHIND_PAWN_BONUS},
        {"6k1/4b3/4p3/8/8/4P3/8/6K1 w - - 0 1", -MINOR_BEHIND_PAWN_BONUS},
        {"4k3/8/8/4p3/4P3/8/4B3/4K3 w - - 0 1", BISHOP_PAWN_BLOCKER_PENALTY * 2},
        {"4k3/8/5b2/4p3/8/8/8/4K3 w - - 0 1", -BISHOP_PAWN_BLOCKER_PENALTY},
    };

    for (const auto& [fen, expected] : testCases) {
        testPiecesEval(fen, expected);
    }
}