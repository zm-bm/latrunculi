#include "evaluator.hpp"

#include <gtest/gtest.h>

#include "chess.hpp"
#include "score.hpp"

class EvaluatorTest : public ::testing::Test {
   protected:
    void testScaleFactor(const std::string& fen, int expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.scaleFactor(), expected) << fen;
    }

    void testPawnsEval(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.pawnsEval<WHITE>(), expectedWhite) << fen;
        EXPECT_EQ(evaluator.pawnsEval<BLACK>(), expectedBlack) << fen;
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

    void testKnightOutpostCount(const std::string& fen, int expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.knightOutpostCount(), expected) << fen;
    }

    void testBishopOutpostCount(const std::string& fen, int expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.bishopOutpostCount(), expected) << fen;
    }

    void testMinorsBehindPawns(const std::string& fen, int expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.minorsBehindPawns(), expected) << fen;
    }

    void testBishopPawnBlockers(const std::string& fen, int expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.bishopPawnBlockers(), expected) << fen;
    }

    void testOutpostSquares(const std::string& fen, U64 expectedWhite, U64 expectedBlack) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.outpostSquares<WHITE>(), expectedWhite) << fen;
        EXPECT_EQ(evaluator.outpostSquares<BLACK>(), expectedBlack) << fen;
    }

    void testHasOppositeBishops(const std::string& fen, bool expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.hasOppositeBishops(), expected);
    }

    void testPhase(const std::string& fen, int expected, int tolerance) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        int phaseValue = evaluator.phase();
        EXPECT_LE(std::abs(phaseValue - expected), tolerance) << fen;
    }

    void testNonPawnMaterial(const std::string& fen, Color c, int expected) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        EXPECT_EQ(evaluator.nonPawnMaterial(c), expected);
    }
};

TEST_F(EvaluatorTest, Eval) {
    std::vector<std::tuple<std::string, int, bool>> testCases = {
        {STARTFEN, 0, true},
        {EMPTYFEN, 0, true},
        {"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", 0, false},
    };

    for (const auto& [fen, expected, exact] : testCases) {
        Chess chess(fen);
        Evaluator evaluator(chess);
        if (exact) {
            EXPECT_EQ(evaluator.eval(), expected + TEMPO_BONUS) << fen;
        } else {
            EXPECT_GT(evaluator.eval(), expected + TEMPO_BONUS) << fen;
        }
        chess.makeNull();
        if (exact) {
            EXPECT_EQ(evaluator.eval(), expected - TEMPO_BONUS) << fen;
        } else {
            EXPECT_LT(evaluator.eval(), expected - TEMPO_BONUS) << fen;
        }
    }
}

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
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        // sanity check
        {STARTFEN, Score{0}, Score{0}},
        {EMPTYFEN, Score{0}, Score{0}},
        // isolated pawns
        {"4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1", ISO_PAWN_PENALTY, ISO_PAWN_PENALTY},
        {"rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 2", ISO_PAWN_PENALTY, Score{0}},
        {"rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 3", Score{0}, ISO_PAWN_PENALTY},
        // backwards pawns
        {"4k3/8/3p4/2p5/2P5/1P6/8/4K3 w - - 0 4", BACKWARD_PAWN_PENALTY, BACKWARD_PAWN_PENALTY},
        {"4k3/8/8/2pp4/2P5/1P6/8/4K3 w - - 0 5", BACKWARD_PAWN_PENALTY, Score{0}},
        {"4k3/8/3p4/2p5/1PP5/8/8/4K3 w - - 0 6", Score{0}, BACKWARD_PAWN_PENALTY},
        // doubled pawns
        {"4k3/5pp1/4p3/3pp3/3PP3/4P3/5PP1/4K3 w - - 0 7", DOUBLED_PAWN_PENALTY, DOUBLED_PAWN_PENALTY},
        {"4k3/5pp1/4p3/3p4/3PP3/4P3/5PP1/4K3 w - - 0 8", DOUBLED_PAWN_PENALTY, Score{0}},
        {"4k3/5pp1/4p3/3pp3/3P4/4P3/5PP1/4K3 w - - 0 9", Score{0}, DOUBLED_PAWN_PENALTY},
        // more complex
        {"k7/8/8/8/8/P7/P7/K7 w KQkq - 0 10", ISO_PAWN_PENALTY * 2 + DOUBLED_PAWN_PENALTY, Score{0}},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPawnsEval(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvaluatorTest, PiecesEval) {
    std::vector<std::pair<std::string, Score>> testCases = {
        {STARTFEN, Score{0}},
        {EMPTYFEN, Score{0}},
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

TEST_F(EvaluatorTest, KnightEval) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, Score{0}, Score{0}},
        {EMPTYFEN, Score{0}, Score{0}},
        {"6k1/8/2p1n3/4p3/4P3/2P1N3/8/6K1 w - - 0 1",
         REACHABLE_OUTPOST_BONUS,
         REACHABLE_OUTPOST_BONUS}};

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testKnightEval(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvaluatorTest, BishopEval) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, Score{0}, Score{0}},
        {EMPTYFEN, Score{0}, Score{0}},
        {"4k3/ppp3p1/5n2/2b1pb2/8/1PP5/1B3PB1/4K3 w - - 0 1",
         BISHOP_PAWN_XRAY_PENALTY * 3,
         BISHOP_PAWN_XRAY_PENALTY}};

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testBishopEval(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvaluatorTest, QueenEval) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, Score{0}, Score{0}},
        {EMPTYFEN, Score{0}, Score{0}},
        {"q3k3/r7/8/8/8/R7/R7/Q3K3 w - - 0 1",
         ROOK_ON_QUEEN_FILE_BONUS * 2,
         ROOK_ON_QUEEN_FILE_BONUS},
        {"qr2k3/r7/8/8/8/R7/Q7/QR2K3 w - - 0 1",
         ROOK_ON_QUEEN_FILE_BONUS,
         ROOK_ON_QUEEN_FILE_BONUS},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testQueenEval(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvaluatorTest, KnightOutpostCount) {
    std::vector<std::tuple<std::string, int>> testCases = {
        {STARTFEN, 0},
        {EMPTYFEN, 0},
        {"6k1/8/8/3Np3/3nP3/8/8/6K1 w - - 0 1", 0},
        {"6k1/8/8/3Np3/4P3/8/8/6K1 w - - 0 1", 1},
        {"6k1/8/8/4p3/3nP3/8/8/6K1 w - - 0 1", -1},
    };

    for (const auto& [fen, expected] : testCases) {
        testKnightOutpostCount(fen, expected);
    }
}

TEST_F(EvaluatorTest, BishopOutpostCount) {
    std::vector<std::tuple<std::string, int>> testCases = {
        {STARTFEN, 0},
        {EMPTYFEN, 0},
        {"6k1/8/8/3Bp3/3bP3/8/8/6K1 w - - 0 1", 0},
        {"6k1/8/8/3Bp3/4P3/8/8/6K1 w - - 0 1", 1},
        {"6k1/8/8/4p3/3bP3/8/8/6K1 w - - 1 1", -1},
    };

    for (const auto& [fen, expected] : testCases) {
        testBishopOutpostCount(fen, expected);
    }
}

TEST_F(EvaluatorTest, MinorsBehindPawns) {
    std::vector<std::tuple<std::string, int>> testCases = {
        {STARTFEN, 0},
        {EMPTYFEN, 0},
        {"4k3/8/8/4p3/4P3/4N3/8/4K3 w - - 0 1", 1},
        {"4k3/8/4b3/4p3/4P3/8/8/4K3 w - - 0 1", -1},
    };

    for (const auto& [fen, expected] : testCases) {
        testMinorsBehindPawns(fen, expected);
    }
}

TEST_F(EvaluatorTest, BishopPawnBlockers) {
    std::vector<std::tuple<std::string, int>> testCases = {
        {STARTFEN, 0},
        {EMPTYFEN, 0},
        {"rn1qkbnr/ppp1pppp/3p4/8/8/4P3/PPPP1PPP/RN1QKBNR w KQkq - 0 1", -2},
        {"4kb2/5p1p/pp2p1p1/2pp4/2PP4/1P2PP1P/P5P1/4KB2 w - - 0 1", 12},
        {"6k1/8/8/3Bp3/4P3/8/8/6K1 w - - 0 1", 1},
    };

    for (const auto& [fen, expected] : testCases) {
        testBishopPawnBlockers(fen, expected);
    }
}

TEST_F(EvaluatorTest, OutpostSquares) {
    std::vector<std::tuple<std::string, U64, U64>> testCases = {
        {STARTFEN, 0, 0},
        {EMPTYFEN, 0, 0},
        {"r4rk1/1p2pppp/1P1pn3/2p5/8/pNPPP3/P4PPP/2KRR3 w - - 0 1", 0, 0},
        {"r4rk1/pp3ppp/3p2n1/2p5/4P3/2N5/PPP2PPP/2KRR3 w - - 0 1", BB::set(D5), 0},
        {"r4rk1/pp2pppp/3pn3/2p5/2P1P3/1N6/PP3PPP/2KRR3 w - - 0 1", 0, BB::set(D4)},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testOutpostSquares(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvaluatorTest, HasOpppositeBishops) {
    std::vector<std::tuple<std::string, bool>> testCases = {
        {STARTFEN, false},
        {EMPTYFEN, false},
        {"3bk3/8/8/8/8/8/8/2B1K3 w - - 0 1", false},
        {"3bk3/8/8/8/8/8/8/3BK3 w - - 0 1", true},
    };

    for (const auto& [fen, expected] : testCases) {
        testHasOppositeBishops(fen, expected);
    }
}

TEST_F(EvaluatorTest, Phase) {
    std::vector<std::tuple<std::string, int, int>> testCases = {
        {STARTFEN, PHASE_LIMIT, 0},
        {EMPTYFEN, 0, 0},
        {"r1n1k2r/8/8/8/8/8/8/R2QKB2 w Kkq - 0 1", 50, 10},
        {"r1n1k3/8/8/8/8/8/8/4KB1R w Kkq - 0 1", 0, 0},
    };

    for (const auto& [fen, expected, tolerance] : testCases) {
        testPhase(fen, expected, tolerance);
    }
}

TEST_F(EvaluatorTest, NonPawnMaterial) {
    int mat = 2 * KNIGHT_VALUE_MG + 2 * BISHOP_VALUE_MG + 2 * ROOK_VALUE_MG + QUEEN_VALUE_MG;
    std::vector<std::tuple<std::string, Color, int>> testCases = {
        {EMPTYFEN, WHITE, 0},
        {EMPTYFEN, BLACK, 0},
        {STARTFEN, WHITE, mat},
        {STARTFEN, BLACK, mat},
    };

    for (const auto& [fen, expected, tolerance] : testCases) {
        testNonPawnMaterial(fen, expected, tolerance);
    }
}
