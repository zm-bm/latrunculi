#include "eval.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "board.hpp"
#include "constants.hpp"
#include "score.hpp"
#include "types.hpp"

using Conf = Eval::Conf;

class EvalTest : public ::testing::Test {
   protected:
    void testOutpostZones(const std::string& fen, U64 expectedWhite, U64 expectedBlack) {
        Board board(fen);
        Eval eval(board);
        EXPECT_EQ(eval.zones.outposts[WHITE], expectedWhite) << fen;
        EXPECT_EQ(eval.zones.outposts[BLACK], expectedBlack) << fen;
    }

    void testMobilityZones(const std::string& fen, U64 expectedWhite, U64 expectedBlack) {
        Board board(fen);
        Eval eval(board);
        EXPECT_EQ(eval.zones.mobility[WHITE], expectedWhite) << fen;
        EXPECT_EQ(eval.zones.mobility[BLACK], expectedBlack) << fen;
    }

    void testMobilityScore(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Board board(fen);
        Eval eval(board);
        eval.evaluate();
        EXPECT_EQ(eval.scores.mobility[WHITE], expectedWhite) << fen;
        EXPECT_EQ(eval.scores.mobility[BLACK], expectedBlack) << fen;
    }

    void testEvaluatePawns(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Board board(fen);
        Eval eval(board);
        EXPECT_EQ(eval.evaluatePawns<WHITE>(), expectedWhite) << fen;
        EXPECT_EQ(eval.evaluatePawns<BLACK>(), expectedBlack) << fen;
    }

    template <PieceType p>
    void testEvaluatePieces(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Board board(fen);
        Eval eval(board);
        Score wScore = eval.evaluatePieces<WHITE, p>();
        Score bScore = eval.evaluatePieces<BLACK, p>();
        EXPECT_EQ(wScore, expectedWhite) << fen;
        EXPECT_EQ(bScore, expectedBlack) << fen;
    }

    void testKingScore(const std::string& fen, Score expected) {
        Board board(fen);
        Eval eval(board);
        eval.evaluate();
        EXPECT_EQ(eval.evaluateKing<WHITE>(), expected) << fen;
        EXPECT_EQ(eval.evaluateKing<BLACK>(), expected) << fen;
    }

    void testKingShelter(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Board board(fen);
        Eval eval(board);
        EXPECT_EQ(eval.kingShelter<WHITE>(board.kingSq(WHITE)), expectedWhite) << fen;
        EXPECT_EQ(eval.kingShelter<BLACK>(board.kingSq(BLACK)), expectedBlack) << fen;
    }

    void testFileShelter(const std::string& fen,
                         Score expectedWhite,
                         Score expectedBlack,
                         File file) {
        Board board(fen);
        Eval eval(board);
        U64 wPawns = board.pieces<PieceType::Pawn>(WHITE);
        U64 bPawns = board.pieces<PieceType::Pawn>(BLACK);
        EXPECT_EQ(eval.kingFileShelter<WHITE>(wPawns, bPawns, file), expectedWhite) << fen;
        EXPECT_EQ(eval.kingFileShelter<BLACK>(bPawns, wPawns, file), expectedBlack) << fen;
    }

    void testRawKingDanger(const std::string& fen, int expectedWhite, int expectedBlack) {
        Board board(fen);
        Eval eval(board);
        eval.evaluate();
        EXPECT_EQ(eval.kingDangerRaw<WHITE>(board.kingSq(WHITE)), expectedWhite) << fen;
        EXPECT_EQ(eval.kingDangerRaw<BLACK>(board.kingSq(BLACK)), expectedBlack) << fen;
    }

    void testPhase(const std::string& fen, int expected, int tolerance) {
        Board board(fen);
        Eval eval(board);
        int phaseValue = eval.phase();
        EXPECT_LE(std::abs(phaseValue - expected), tolerance) << fen;
    }

    void testScaleFactor(const std::string& fen, float expected) {
        Board board(fen);
        Eval eval(board);
        EXPECT_EQ(eval.scaleFactor(), expected) << fen;
    }

    void testTaperScore(const std::string& fen, Score score, int expected) {
        Board board(fen);
        Eval eval(board);
        int result = eval.taperScore(score);
        EXPECT_EQ(result, expected) << fen;
    }
};

TEST_F(EvalTest, Eval) {
    std::vector<std::tuple<std::string, int>> testCases = {
        {EMPTYFEN, 0},
        {STARTFEN, 0},
    };

    for (const auto& [fen, expected] : testCases) {
        Board board(fen);
        int result = eval(board);
        EXPECT_EQ(result, expected + TEMPO_BONUS) << fen;
        board.makeNull();
        EXPECT_EQ(result, expected + TEMPO_BONUS) << fen;
    }
}

TEST_F(EvalTest, Outposts) {
    std::vector<std::tuple<std::string, U64, U64>> testCases = {
        {STARTFEN, 0, 0},
        {EMPTYFEN, 0, 0},
        {"r4rk1/1p2pppp/1P1pn3/2p5/8/pNPPP3/P4PPP/2KRR3 w - - 0 1", 0, 0},
        {"r4rk1/pp3ppp/3p2n1/2p5/4P3/2N5/PPP2PPP/2KRR3 w - - 0 1", BB::set(D5), 0},
        {"r4rk1/pp2pppp/3pn3/2p5/2P1P3/1N6/PP3PPP/2KRR3 w - - 0 1", 0, BB::set(D4)},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testOutpostZones(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, MobilityArea) {
    U64 white = BB::rankBB(Rank::R2) | BB::rankBB(Rank::R6) | BB::set(KingOrigin[WHITE]);
    U64 black = BB::rankBB(Rank::R7) | BB::rankBB(Rank::R3) | BB::set(KingOrigin[BLACK]);

    std::vector<std::tuple<std::string, U64, U64>> testCases = {
        {STARTFEN, ~white, ~black},
        {EMPTYFEN, ~BB::set(KingOrigin[WHITE]), ~BB::set(KingOrigin[BLACK])},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testMobilityZones(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, MobilityScore) {
    std::vector<std::tuple<std::string, Score>> testCases = {
        {EMPTYFEN, {0}},
        // no mobility area restriction
        {"3nk3/8/8/8/8/8/8/3NK3 w - - 0 1", Conf::KnightMobility[4]},
        {"3bk3/8/8/8/8/8/8/3BK3 w - - 0 2", Conf::BishopMobility[7]},
        {"3rk3/8/8/8/8/8/8/3RK3 w - - 0 3", Conf::RookMobility[10]},
        {"3qk3/8/8/8/8/8/8/3QK3 w - - 0 4", Conf::QueenMobility[17]},
        // with mobility area restriction
        {"3nk3/1p6/8/3P4/3p4/8/1P6/3NK3 w - - 0 5", Conf::KnightMobility[1]},
        {"3bk3/4p3/8/1p6/1P6/8/4P3/3BK3 w - - 0 6", Conf::BishopMobility[2]},
        {"3rk3/P2p4/8/8/8/8/p2P4/3RK3 w - - 0 7", Conf::RookMobility[2]},
        {"3qk3/P2pp3/8/1p6/1P6/8/p2PP3/3QK3 w - - 0 8", Conf::QueenMobility[4]},

    };

    for (const auto& [fen, expected] : testCases) {
        testMobilityScore(fen, expected, expected);
    }
}

TEST_F(EvalTest, EvaluatePawns) {
    auto isoPawn1       = "4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1";
    auto isoPawn2       = "rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 2";
    auto isoPawn3       = "rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 3";
    auto backwardPawn1  = "4k3/8/3p4/2p5/2P5/1P6/8/4K3 w - - 0 4";
    auto backwardPawn2  = "4k3/8/8/2pp4/2P5/1P6/8/4K3 w - - 0 5";
    auto backwardPawn3  = "4k3/8/3p4/2p5/1PP5/8/8/4K3 w - - 0 6";
    auto doubledPawn1   = "4k3/5pp1/4p3/3p4/3PP3/4P3/5PP1/4K3 w - - 0 7";
    auto doubledPawn2   = "4k3/5pp1/4p3/3pp3/3P4/4P3/5PP1/4K3 w - - 0 8";
    auto isoDoubledPawn = "k7/8/8/8/8/P7/P7/K7 w KQkq - 0 9";

    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        // sanity check
        {EMPTYFEN, ZERO_SCORE, ZERO_SCORE},
        {STARTFEN, ZERO_SCORE, ZERO_SCORE},
        // isolated pawns
        {isoPawn1, Conf::IsoPawn, Conf::IsoPawn},
        {isoPawn2, Conf::IsoPawn, ZERO_SCORE},
        {isoPawn3, ZERO_SCORE, Conf::IsoPawn},
        // backwards pawns
        {backwardPawn1, Conf::BackwardPawn, Conf::BackwardPawn},
        {backwardPawn2, Conf::BackwardPawn, ZERO_SCORE},
        {backwardPawn3, ZERO_SCORE, Conf::BackwardPawn},
        // doubled pawns
        {doubledPawn1, Conf::DoubledPawn, ZERO_SCORE},
        {doubledPawn2, ZERO_SCORE, Conf::DoubledPawn},
        // isolated and doubled pawns
        {isoDoubledPawn, Conf::IsoPawn * 2 + Conf::DoubledPawn, ZERO_SCORE},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testEvaluatePawns(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, KnightsScore) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {EMPTYFEN, ZERO_SCORE, ZERO_SCORE},
        {STARTFEN, Conf::MinorPawnShield * 2, Conf::MinorPawnShield * 2},
        // knight outposts
        {"6k1/8/2p5/4pNp1/3nP1P1/2P5/8/6K1 w - - 0 1", Conf::KnightOutpost, ZERO_SCORE},
        {"6k1/8/2p5/3Np1p1/4PnP1/2P5/8/6K1 w - - 0 2", ZERO_SCORE, Conf::KnightOutpost},
        // knight with reachable outposts
        {"6k1/8/2p5/1n2p1p1/4P1PN/2P5/8/6K1 w - - 0 3", Conf::ReachableOutpost, ZERO_SCORE},
        {"6k1/8/2p5/4p1pn/1N2P1P1/2P5/8/6K1 w - - 0 4", ZERO_SCORE, Conf::ReachableOutpost},
        // knight behind pawn
        {"6k1/8/4p3/8/8/4P3/4N3/6K1 w - - 0 5", Conf::MinorPawnShield, ZERO_SCORE},
        {"6k1/4n3/4p3/8/8/4P3/8/6K1 w - - 0 6", ZERO_SCORE, Conf::MinorPawnShield},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testEvaluatePieces<PieceType::Knight>(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, BishopsScore) {
    Score startScore = Conf::MinorPawnShield * 2 + Conf::BishopPair + Conf::BishopBlockedByPawn * 8,
          hasOutpost = Conf::BishopOutpost + Conf::BishopBlockedByPawn * 2,
          noOutpost  = Conf::BishopBlockedByPawn * 4,
          hasLongDiag        = Conf::BishopLongDiagonal + Conf::BishopBlockedByPawn,
          noLongDiag         = Conf::BishopBlockedByPawn * 2,
          twoPawnsDefended   = Conf::BishopBlockedByPawn * 2 + Conf::BishopOutpost,
          twoPawnsOneBlocked = Conf::BishopBlockedByPawn * 4,
          twoPawnsTwoBlocked = Conf::BishopBlockedByPawn * 6;

    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {EMPTYFEN, ZERO_SCORE, ZERO_SCORE},
        {STARTFEN, startScore, startScore},
        // bishop outposts
        {"6k1/8/2p5/4pBp1/4P1P1/2P3b1/8/6K1 w - - 0 1", hasOutpost, noOutpost},
        {"6k1/8/2p3B1/4p1p1/4PbP1/2P5/8/6K1 w - - 0 2", noOutpost, hasOutpost},
        // bishop behind pawn
        {"6k1/8/4p3/8/8/4P3/4B3/6K1 w - - 0 3", Conf::MinorPawnShield, ZERO_SCORE},
        {"6k1/4b3/4p3/8/8/4P3/8/6K1 w - - 0 4", ZERO_SCORE, Conf::MinorPawnShield},
        // bishop on long diagonal
        {"6k1/6b1/8/3P4/3p4/8/6B1/6K1 w - - 0 5", hasLongDiag, hasLongDiag},
        {"6k1/6b1/8/4p3/4P3/8/6B1/6K1 w - - 0 6", noLongDiag, noLongDiag},
        // bishop pair
        {"5bk1/8/8/8/8/8/8/4BBK1 w - - 0 7", Conf::BishopPair, ZERO_SCORE},
        {"4bbk1/8/8/8/8/8/8/5BK1 w - - 0 8", ZERO_SCORE, Conf::BishopPair},
        // bishop/pawn penalty
        {"4k3/8/8/2BPp3/2bpP3/8/8/4K3 w - - 0 9", ZERO_SCORE, ZERO_SCORE},
        {"4k3/8/8/2bPp3/2BpP3/8/8/4K3 w - - 0 10", twoPawnsOneBlocked, twoPawnsOneBlocked},
        {"4k3/8/8/3PpB2/3pPb2/8/8/4K3 w - - 0 11", twoPawnsDefended, twoPawnsDefended},
        {"4k3/4b3/8/4p3/3pP3/3P4/4B3/4K3 w - - 0 12", twoPawnsTwoBlocked, twoPawnsTwoBlocked},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testEvaluatePieces<PieceType::Bishop>(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, RookScore) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, ZERO_SCORE, ZERO_SCORE},
        {EMPTYFEN, ZERO_SCORE, ZERO_SCORE},
        {"6kr/8/8/8/8/8/8/RK6 w - - 0 1", Conf::RookOpenFile[1], Conf::RookOpenFile[1]},
        {"6kr/p7/8/8/8/8/7P/RK6 w - - 0 2", Conf::RookOpenFile[0], Conf::RookOpenFile[0]},
        {"rn5k/8/8/p7/P7/8/8/RN5K w - - 0 3", Conf::RookClosedFile, Conf::RookClosedFile},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testEvaluatePieces<PieceType::Rook>(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, QueenScore) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, ZERO_SCORE, ZERO_SCORE},
        {EMPTYFEN, ZERO_SCORE, ZERO_SCORE},
        // bishop discovered attack
        {"3qk3/2P5/1P6/B7/b7/1p6/8/3QK3 w - - 0 1", Conf::QueenDiscoveredAttack, ZERO_SCORE},
        {"3qk3/8/1P6/B7/b7/1p6/2p5/3QK3 w - - 0 2", ZERO_SCORE, Conf::QueenDiscoveredAttack},
        // rook discovered attack
        {"RNNqk3/8/8/8/8/8/8/rn1QK3 w - - 0 3", Conf::QueenDiscoveredAttack, ZERO_SCORE},
        {"RN1qk3/8/8/8/8/8/8/rnnQK3 w - - 0 4", ZERO_SCORE, Conf::QueenDiscoveredAttack},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testEvaluatePieces<PieceType::Queen>(fen, expectedWhite, expectedBlack);
    }
}

Score calculateShelter(const std::vector<Rank>& shelterRanks,
                       const std::vector<Rank>& stormRanks,
                       const std::vector<Rank>& blockedRanks) {
    Score score;
    for (auto r : shelterRanks) score += Conf::PawnRankShelter[idx(r)];
    for (auto r : stormRanks) score += Conf::PawnRankStorm[0][idx(r)];
    for (auto r : blockedRanks) score += Conf::PawnRankStorm[1][idx(r)];
    return score;
}

TEST_F(EvalTest, KingScore) {
    Score empty =
        calculateShelter({Rank::R1, Rank::R1, Rank::R1}, {Rank::R1, Rank::R1, Rank::R1}, {}) +
        Conf::KingFile[idx(File::F5)] + Conf::KingOpenFile[true][true];
    Score start =
        calculateShelter({Rank::R2, Rank::R2, Rank::R2}, {Rank::R7, Rank::R7, Rank::R7}, {}) +
        Conf::KingFile[idx(File::F7)] + Conf::KingOpenFile[false][false];

    std::vector<std::tuple<std::string, Score>> testCases = {
        {EMPTYFEN, empty},
        {STARTFEN, start},
    };

    for (const auto& [fen, expected] : testCases) {
        testKingScore(fen, expected);
    }
}

TEST_F(EvalTest, KingShelter) {
    Score empty =
        calculateShelter({Rank::R1, Rank::R1, Rank::R1}, {Rank::R1, Rank::R1, Rank::R1}, {}) +
        Conf::KingFile[int(File::F5)] + Conf::KingOpenFile[true][true];
    Score start =
        calculateShelter({Rank::R2, Rank::R2, Rank::R2}, {Rank::R7, Rank::R7, Rank::R7}, {}) +
        Conf::KingFile[int(File::F5)] + Conf::KingOpenFile[false][false];
    Score blockedPawn =
        calculateShelter({Rank::R3, Rank::R4, Rank::R5}, {Rank::R6, Rank::R4}, {Rank::R5}) +
        Conf::KingFile[int(File::F1)] + Conf::KingOpenFile[false][false];
    Score semiOpenFile1 =
        calculateShelter({Rank::R2, Rank::R2, Rank::R2}, {Rank::R1, Rank::R1, Rank::R1}, {}) +
        Conf::KingFile[int(File::F1)] + Conf::KingOpenFile[false][true];
    Score semiOpenFile2 =
        calculateShelter({Rank::R1, Rank::R1, Rank::R1}, {Rank::R7, Rank::R7, Rank::R7}, {}) +
        Conf::KingFile[int(File::F1)] + Conf::KingOpenFile[true][false];
    Score kingOnRank2 =
        calculateShelter({Rank::R1, Rank::R1, Rank::R3}, {Rank::R7, Rank::R7, Rank::R6}, {}) +
        Conf::KingFile[int(File::F2)] + Conf::KingOpenFile[false][false];
    Score attackedPawn =
        calculateShelter({Rank::R2, Rank::R2, Rank::R1}, {Rank::R7, Rank::R7, Rank::R7}, {}) +
        Conf::KingFile[int(File::F1)] + Conf::KingOpenFile[false][false];

    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {EMPTYFEN, empty, empty},
        {STARTFEN, start, start},
        {"k7/8/p7/1pP5/1Pp5/P7/8/K7 w - - 0 1", blockedPawn, blockedPawn},
        {"7k/5ppp/8/8/8/8/PPP5/K7 w - - 0 2", semiOpenFile1, semiOpenFile1},
        {"k7/5ppp/8/8/8/8/PPP5/7K w - - 0 3", semiOpenFile2, semiOpenFile2},
        {"8/5pkp/6p1/8/8/6P1/5PKP/8 w - - 0 4", kingOnRank2, kingOnRank2},
        {"k7/ppp5/3P4/8/8/3p4/PPP5/K7 w - - 0 5", attackedPawn, attackedPawn},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testKingShelter(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, FileShelter) {
    Score empty       = calculateShelter({Rank::R1}, {Rank::R1}, {});
    Score start       = calculateShelter({Rank::R2}, {Rank::R7}, {});
    Score blockedPawn = calculateShelter({Rank::R4}, {}, {Rank::R5});

    std::vector<std::tuple<std::string, Score, Score, File>> testCases = {
        {EMPTYFEN, empty, empty, File::F5},
        {STARTFEN, start, start, File::F5},
        {"1k6/8/8/1p6/1P6/8/8/1K6 w - - 0 1", blockedPawn, blockedPawn, File::F2},
    };

    for (const auto& [fen, expectedWhite, expectedBlack, file] : testCases) {
        testFileShelter(fen, expectedWhite, expectedBlack, file);
    }
}

TEST_F(EvalTest, KingDangerRaw) {
    int bigDanger =
        (Conf::SafeCheckDanger[PieceIdx::Queen] + Conf::SafeCheckDanger[PieceIdx::Bishop] +
         Conf::AttackedKingZoneDanger[PieceIdx::Queen] + Conf::WeakKingZoneDanger);

    std::vector<std::tuple<std::string, int, int>> testCases = {
        // No danger
        {EMPTYFEN, 0, 0},
        {STARTFEN, 0, 0},
        // unsafe rook checks
        {"4k3/5n2/8/8/8/8/4P3/4K1NR w - - 0 2", 0, Conf::UnsafeCheckDanger[PieceIdx::Rook]},
        {"4k1nr/4p3/8/8/8/8/5N2/4K3 w - - 0 3", Conf::UnsafeCheckDanger[PieceIdx::Rook], 0},
        // safe queen + bishop checks
        {"r1n1kn1r/8/8/8/8/8/8/R2QKB2 w - - 0 4", 0, bigDanger},
        {"r2qkb2/8/8/8/8/8/8/R1N1KN1R w - - 0 5", bigDanger, 0},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testRawKingDanger(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, ScaleFactor) {
    std::vector<std::pair<std::string, float>> testCases = {
        {EMPTYFEN, 36.0 / SCALE_LIMIT},
        {STARTFEN, 1.0},
        {"4k3/8/8/8/8/8/4P3/4K3 w K - 0 1", 41.0 / SCALE_LIMIT},  // Single pawn
    };

    for (const auto& [fen, expected] : testCases) {
        testScaleFactor(fen, expected);
    }
}

TEST_F(EvalTest, TaperScore) {
    std::vector<std::tuple<std::string, Score, int>> testCases = {
        {EMPTYFEN, {100, 200}, 200},
        {STARTFEN, {100, 200}, 100},
    };

    for (const auto& [fen, score, expected] : testCases) {
        testTaperScore(fen, score, expected);
    }
}

TEST_F(EvalTest, Phase) {
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
