#include "eval.hpp"

#include <gtest/gtest.h>

#include "bb.hpp"
#include "board.hpp"
#include "constants.hpp"
#include "score.hpp"
#include "types.hpp"

class EvalTest : public ::testing::Test {
   protected:
    void testOutposts(const std::string& fen, U64 expectedWhite, U64 expectedBlack) {
        Board board(fen);
        Eval<Silent> eval(board);
        EXPECT_EQ(eval.outposts[WHITE], expectedWhite) << fen;
        EXPECT_EQ(eval.outposts[BLACK], expectedBlack) << fen;
    }

    void testMobilityArea(const std::string& fen, U64 expectedWhite, U64 expectedBlack) {
        Board board(fen);
        Eval<Silent> eval(board);
        EXPECT_EQ(eval.mobilityZone[WHITE], expectedWhite) << fen;
        EXPECT_EQ(eval.mobilityZone[BLACK], expectedBlack) << fen;
    }

    void testMobility(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Board board(fen);
        Eval<Silent> eval(board);
        eval.evaluate();
        EXPECT_EQ(eval.mobility[WHITE], expectedWhite) << fen;
        EXPECT_EQ(eval.mobility[BLACK], expectedBlack) << fen;
    }

    void testPawnsScore(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Board board(fen);
        Eval<Silent> eval(board);
        EXPECT_EQ(eval.pawnsScore<WHITE>(), expectedWhite) << fen;
        EXPECT_EQ(eval.pawnsScore<BLACK>(), expectedBlack) << fen;
    }

    template <PieceType p>
    void testPiecesScore(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Board board(fen);
        Eval<Silent> eval(board);
        Score wScore = eval.piecesScore<WHITE, p>();
        Score bScore = eval.piecesScore<BLACK, p>();
        EXPECT_EQ(wScore, expectedWhite) << fen;
        EXPECT_EQ(bScore, expectedBlack) << fen;
    }

    void testKingScore(const std::string& fen, Score expected) {
        Board board(fen);
        Eval<Silent> eval(board);
        eval.evaluate();
        EXPECT_EQ(eval.kingScore<WHITE>(), expected) << fen;
        EXPECT_EQ(eval.kingScore<BLACK>(), expected) << fen;
    }

    void testKingShelter(const std::string& fen, Score expectedWhite, Score expectedBlack) {
        Board board(fen);
        Eval<Silent> eval(board);
        EXPECT_EQ(eval.kingShelter<WHITE>(board.kingSq(WHITE)), expectedWhite) << fen;
        EXPECT_EQ(eval.kingShelter<BLACK>(board.kingSq(BLACK)), expectedBlack) << fen;
    }

    void testFileShelter(const std::string& fen,
                         Score expectedWhite,
                         Score expectedBlack,
                         File file) {
        Board board(fen);
        Eval<Silent> eval(board);
        U64 wPawns = board.pieces<PAWN>(WHITE);
        U64 bPawns = board.pieces<PAWN>(BLACK);
        EXPECT_EQ(eval.fileShelter<WHITE>(wPawns, bPawns, file), expectedWhite) << fen;
        EXPECT_EQ(eval.fileShelter<BLACK>(bPawns, wPawns, file), expectedBlack) << fen;
    }

    void testPhase(const std::string& fen, int expected, int tolerance) {
        Board board(fen);
        Eval<Silent> eval(board);
        int phaseValue = eval.phase();
        EXPECT_LE(std::abs(phaseValue - expected), tolerance) << fen;
    }

    void testNonPawnMaterial(const std::string& fen, Color c, int expected) {
        Board board(fen);
        Eval<Silent> eval(board);
        EXPECT_EQ(eval.nonPawnMaterial(c), expected);
    }

    void testScaleFactor(const std::string& fen, int expected) {
        Board board(fen);
        Eval<Silent> eval(board);
        EXPECT_EQ(eval.scaleFactor(), expected) << fen;
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
        testOutposts(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, MobilityArea) {
    U64 white = BB::rank(RANK2) | BB::rank(RANK6) | BB::set(KingOrigin[WHITE]);
    U64 black = BB::rank(RANK7) | BB::rank(RANK3) | BB::set(KingOrigin[BLACK]);

    std::vector<std::tuple<std::string, U64, U64>> testCases = {
        {STARTFEN, ~white, ~black},
        {EMPTYFEN, ~BB::set(KingOrigin[WHITE]), ~BB::set(KingOrigin[BLACK])},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testMobilityArea(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, Mobility) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {EMPTYFEN, {0}, {0}},
        // no mobility area restriction
        {"3nk3/8/8/8/8/8/8/3NK3 w - - 0 1",
         Eval<>::Scores::KnightMobility[4],
         Eval<>::Scores::KnightMobility[4]},
        {"3bk3/8/8/8/8/8/8/3BK3 w - - 0 2",
         Eval<>::Scores::BishopMobility[7],
         Eval<>::Scores::BishopMobility[7]},
        {"3rk3/8/8/8/8/8/8/3RK3 w - - 0 3",
         Eval<>::Scores::RookMobility[10],
         Eval<>::Scores::RookMobility[10]},
        {"3qk3/8/8/8/8/8/8/3QK3 w - - 0 4",
         Eval<>::Scores::QueenMobility[17],
         Eval<>::Scores::QueenMobility[17]},
        // with mobility area restriction
        {"3nk3/1p6/8/3P4/3p4/8/1P6/3NK3 w - - 0 5",
         Eval<>::Scores::KnightMobility[1],
         Eval<>::Scores::KnightMobility[1]},
        {"3bk3/4p3/8/1p6/1P6/8/4P3/3BK3 w - - 0 6",
         Eval<>::Scores::BishopMobility[2],
         Eval<>::Scores::BishopMobility[2]},
        {"3rk3/P2p4/8/8/8/8/p2P4/3RK3 w - - 0 7",
         Eval<>::Scores::RookMobility[2],
         Eval<>::Scores::RookMobility[2]},
        {"3qk3/P2pp3/8/1p6/1P6/8/p2PP3/3QK3 w - - 0 8",
         Eval<>::Scores::QueenMobility[4],
         Eval<>::Scores::QueenMobility[4]},

    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testMobility(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, PawnsScore) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        // sanity check
        {EMPTYFEN, Score{0}, Score{0}},
        {STARTFEN, Score{0}, Score{0}},

        // isolated pawns
        {"4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1", Eval<>::Scores::IsoPawn, Eval<>::Scores::IsoPawn},
        {"rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 2",
         Eval<>::Scores::IsoPawn,
         Score{0}},
        {"rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 3",
         Score{0},
         Eval<>::Scores::IsoPawn},

        // backwards pawns
        {"4k3/8/3p4/2p5/2P5/1P6/8/4K3 w - - 0 4",
         Eval<>::Scores::BackwardPawn,
         Eval<>::Scores::BackwardPawn},
        {"4k3/8/8/2pp4/2P5/1P6/8/4K3 w - - 0 5", Eval<>::Scores::BackwardPawn, Score{0}},
        {"4k3/8/3p4/2p5/1PP5/8/8/4K3 w - - 0 6", Score{0}, Eval<>::Scores::BackwardPawn},
        // doubled pawns
        {"4k3/5pp1/4p3/3p4/3PP3/4P3/5PP1/4K3 w - - 0 7", Eval<>::Scores::DoubledPawn, Score{0}},
        {"4k3/5pp1/4p3/3pp3/3P4/4P3/5PP1/4K3 w - - 0 8", Score{0}, Eval<>::Scores::DoubledPawn},
        // other
        {"k7/8/8/8/8/P7/P7/K7 w KQkq - 0 10",
         Eval<>::Scores::IsoPawn * 2 + Eval<>::Scores::DoubledPawn,
         Score{0}},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPawnsScore(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, KnightsScore) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {EMPTYFEN, Score{0}, Score{0}},
        {STARTFEN, Eval<>::Scores::MinorPawnShield * 2, Eval<>::Scores::MinorPawnShield * 2},
        // knight outposts
        {"6k1/8/2p5/4pNp1/3nP1P1/2P5/8/6K1 w - - 0 1", Eval<>::Scores::KnightOutpost, Score{0}},
        {"6k1/8/2p5/3Np1p1/4PnP1/2P5/8/6K1 w - - 0 2", Score{0}, Eval<>::Scores::KnightOutpost},
        // knight with reachable outposts
        {"6k1/8/2p5/1n2p1p1/4P1PN/2P5/8/6K1 w - - 0 3", Eval<>::Scores::ReachableOutpost, Score{0}},
        {"6k1/8/2p5/4p1pn/1N2P1P1/2P5/8/6K1 w - - 0 4", Score{0}, Eval<>::Scores::ReachableOutpost},
        // knight behind pawn
        {"6k1/8/4p3/8/8/4P3/4N3/6K1 w - - 0 5", Eval<>::Scores::MinorPawnShield, Score{0}},
        {"6k1/4n3/4p3/8/8/4P3/8/6K1 w - - 0 6", Score{0}, Eval<>::Scores::MinorPawnShield},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPiecesScore<KNIGHT>(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, BishopsScore) {
    Score startScore  = (Eval<>::Scores::MinorPawnShield * 2 + Eval<>::Scores::BishopPair +
                        Eval<>::Scores::BishopBlockedByPawn * 8),
          hasOutpost  = Eval<>::Scores::BishopOutpost + Eval<>::Scores::BishopBlockedByPawn * 2,
          noOutpost   = Eval<>::Scores::BishopBlockedByPawn * 4,
          hasLongDiag = Eval<>::Scores::BishopLongDiagonal + Eval<>::Scores::BishopBlockedByPawn,
          noLongDiag  = Eval<>::Scores::BishopBlockedByPawn * 2,
          twoPawnsDefended =
              Eval<>::Scores::BishopBlockedByPawn * 2 + Eval<>::Scores::BishopOutpost,
          twoPawnsOneBlocked = Eval<>::Scores::BishopBlockedByPawn * 4,
          twoPawnsTwoBlocked = Eval<>::Scores::BishopBlockedByPawn * 6;

    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {EMPTYFEN, Score{0}, Score{0}},
        {STARTFEN, startScore, startScore},
        // bishop outposts
        {"6k1/8/2p5/4pBp1/4P1P1/2P3b1/8/6K1 w - - 0 1", hasOutpost, noOutpost},
        {"6k1/8/2p3B1/4p1p1/4PbP1/2P5/8/6K1 w - - 0 2", noOutpost, hasOutpost},
        // bishop behind pawn
        {"6k1/8/4p3/8/8/4P3/4B3/6K1 w - - 0 3", Eval<>::Scores::MinorPawnShield, Score{0}},
        {"6k1/4b3/4p3/8/8/4P3/8/6K1 w - - 0 4", Score{0}, Eval<>::Scores::MinorPawnShield},
        // bishop on long diagonal
        {"6k1/6b1/8/3P4/3p4/8/6B1/6K1 w - - 0 5", hasLongDiag, hasLongDiag},
        {"6k1/6b1/8/4p3/4P3/8/6B1/6K1 w - - 0 6", noLongDiag, noLongDiag},
        // bishop pair
        {"5bk1/8/8/8/8/8/8/4BBK1 w - - 0 7", Eval<>::Scores::BishopPair, Score{0}},
        {"4bbk1/8/8/8/8/8/8/5BK1 w - - 0 8", Score{0}, Eval<>::Scores::BishopPair},
        // bishop/pawn penalty
        {"4k3/8/8/2BPp3/2bpP3/8/8/4K3 w - - 0 9", Score{0}, Score{0}},
        {"4k3/8/8/2bPp3/2BpP3/8/8/4K3 w - - 0 10", twoPawnsOneBlocked, twoPawnsOneBlocked},
        {"4k3/8/8/3PpB2/3pPb2/8/8/4K3 w - - 0 11", twoPawnsDefended, twoPawnsDefended},
        {"4k3/4b3/8/4p3/3pP3/3P4/4B3/4K3 w - - 0 12", twoPawnsTwoBlocked, twoPawnsTwoBlocked},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPiecesScore<BISHOP>(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, RookScore) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, Score{0}, Score{0}},
        {EMPTYFEN, Score{0}, Score{0}},
        {"6kr/8/8/8/8/8/8/RK6 w - - 0 1",
         Eval<>::Scores::RookOpenFile[1],
         Eval<>::Scores::RookOpenFile[1]},
        {"6kr/p7/8/8/8/8/7P/RK6 w - - 0 2",
         Eval<>::Scores::RookOpenFile[0],
         Eval<>::Scores::RookOpenFile[0]},
        {"rn5k/8/8/p7/P7/8/8/RN5K w - - 0 3",
         Eval<>::Scores::RookClosedFile,
         Eval<>::Scores::RookClosedFile},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPiecesScore<ROOK>(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, QueenScore) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {STARTFEN, Score{0}, Score{0}},
        {EMPTYFEN, Score{0}, Score{0}},
        // bishop discovered attack
        {"3qk3/2P5/1P6/B7/b7/1p6/8/3QK3 w - - 0 1",
         Eval<>::Scores::QueenDiscoveredAttack,
         Score{0}},
        {"3qk3/8/1P6/B7/b7/1p6/2p5/3QK3 w - - 0 2",
         Score{0},
         Eval<>::Scores::QueenDiscoveredAttack},
        // rook discovered attack
        {"RNNqk3/8/8/8/8/8/8/rn1QK3 w - - 0 3", Eval<>::Scores::QueenDiscoveredAttack, Score{0}},
        {"RN1qk3/8/8/8/8/8/8/rnnQK3 w - - 0 4", Score{0}, Eval<>::Scores::QueenDiscoveredAttack},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPiecesScore<QUEEN>(fen, expectedWhite, expectedBlack);
    }
}

Score calculateShelter(const std::vector<int>& shelterRanks,
                       const std::vector<int>& stormRanks,
                       const std::vector<int>& blockedRanks) {
    Score score;
    for (int r : shelterRanks) score += Eval<>::Scores::PawnRankShelter[r];
    for (int r : stormRanks) score += Eval<>::Scores::PawnRankStorm[0][r];
    for (int r : blockedRanks) score += Eval<>::Scores::PawnRankStorm[1][r];
    return score;
}

// TEST_F(EvalTest, KingScore) {
//     Score empty = calculateShelter({0, 0, 0}, {0, 0, 0}, {}) + Eval<>::Scores::KingFile[FILE5] +
//                   Eval<>::Scores::KingOpenFileScore[true][true];
//     Score start = calculateShelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {}) +
//                   Eval<>::Scores::KingFile[FILE7] +
//                   Eval<>::Scores::KingOpenFileScore[false][false];

//     std::vector<std::tuple<std::string, Score>> testCases = {
//         {EMPTYFEN, empty},
//         {STARTFEN, start},
//         {"1N2k3/8/8/8/8/8/8/1n2K3 w - - 0 1", empty + KING_DANGER[KNIGHT]},
//         {"1B2k3/8/8/8/8/8/8/1b2K3 w - - 0 1", empty + KING_DANGER[BISHOP]},
//         {"1R1nk3/8/8/8/8/8/8/1r1NK3 w - - 0 1", empty + KING_DANGER[ROOK]},
//         {"1Q1nk3/8/8/8/8/8/8/1q1NK3 w - - 0 1", empty + KING_DANGER[QUEEN] * 2},
//     };

//     for (const auto& [fen, expected] : testCases) {
//         testKingScore(fen, expected);
//     }
// }

TEST_F(EvalTest, KingShelter) {
    Score empty = calculateShelter({0, 0, 0}, {0, 0, 0}, {}) + Eval<>::Scores::KingFile[FILE5] +
                  Eval<>::Scores::KingOpenFile[true][true];
    Score start = calculateShelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {}) +
                  Eval<>::Scores::KingFile[FILE5] + Eval<>::Scores::KingOpenFile[false][false];
    Score blockedPawn = calculateShelter({RANK3, RANK4, RANK5}, {RANK6, RANK4}, {RANK5}) +
                        Eval<>::Scores::KingFile[FILE1] +
                        Eval<>::Scores::KingOpenFile[false][false];
    Score semiOpenFile1 = calculateShelter({RANK2, RANK2, RANK2}, {0, 0, 0}, {}) +
                          Eval<>::Scores::KingFile[FILE1] +
                          Eval<>::Scores::KingOpenFile[false][true];
    Score semiOpenFile2 = calculateShelter({0, 0, 0}, {RANK7, RANK7, RANK7}, {}) +
                          Eval<>::Scores::KingFile[FILE1] +
                          Eval<>::Scores::KingOpenFile[true][false];
    Score kingOnRank2 = calculateShelter({0, 0, RANK3}, {RANK7, RANK7, RANK6}, {}) +
                        Eval<>::Scores::KingFile[FILE2] +
                        Eval<>::Scores::KingOpenFile[false][false];
    Score attackedPawn = calculateShelter({RANK2, RANK2, 0}, {RANK7, RANK7, RANK7}, {}) +
                         Eval<>::Scores::KingFile[FILE1] +
                         Eval<>::Scores::KingOpenFile[false][false];

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
    Score empty       = calculateShelter({0}, {0}, {});
    Score start       = calculateShelter({RANK2}, {RANK7}, {});
    Score blockedPawn = calculateShelter({RANK4}, {}, {RANK5});

    std::vector<std::tuple<std::string, Score, Score, File>> testCases = {
        {EMPTYFEN, empty, empty, FILE5},
        {STARTFEN, start, start, FILE5},
        {"1k6/8/8/1p6/1P6/8/8/1K6 w - - 0 1", blockedPawn, blockedPawn, FILE2},
    };

    for (const auto& [fen, expectedWhite, expectedBlack, file] : testCases) {
        testFileShelter(fen, expectedWhite, expectedBlack, file);
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

TEST_F(EvalTest, NonPawnMaterial) {
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

TEST_F(EvalTest, ScaleFactor) {
    std::vector<std::pair<std::string, int>> testCases = {
        {EMPTYFEN, 36},
        {STARTFEN, SCALE_LIMIT},
    };

    for (const auto& [fen, expected] : testCases) {
        testScaleFactor(fen, expected);
    }
}
