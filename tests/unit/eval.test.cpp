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
    std::vector<std::tuple<std::string, int, bool>> testCases = {
        {EMPTYFEN, 0, true},
        {STARTFEN, 0, true},
        {"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", 0, false},
    };

    for (const auto& [fen, expected, exact] : testCases) {
        Board board(fen);
        Eval<Silent> eval(board);
        if (exact) {
            EXPECT_EQ(eval.evaluate(), expected + TEMPO_BONUS) << fen;
        } else {
            EXPECT_GT(eval.evaluate(), expected + TEMPO_BONUS) << fen;
        }
        board.makeNull();
        if (exact) {
            EXPECT_EQ(eval.evaluate(), expected + TEMPO_BONUS) << fen;
        } else {
            EXPECT_LT(eval.evaluate(), expected + TEMPO_BONUS) << fen;
        }
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
        {"3nk3/8/8/8/8/8/8/3NK3 w - - 0 1", KNIGHT_MOBILITY[4], KNIGHT_MOBILITY[4]},
        {"3bk3/8/8/8/8/8/8/3BK3 w - - 0 2", BISHOP_MOBILITY[7], BISHOP_MOBILITY[7]},
        {"3rk3/8/8/8/8/8/8/3RK3 w - - 0 3", ROOK_MOBILITY[10], ROOK_MOBILITY[10]},
        {"3qk3/8/8/8/8/8/8/3QK3 w - - 0 4", QUEEN_MOBILITY[17], QUEEN_MOBILITY[17]},
        // with mobility area restriction
        {"3nk3/1p6/8/3P4/3p4/8/1P6/3NK3 w - - 0 5", KNIGHT_MOBILITY[1], KNIGHT_MOBILITY[1]},
        {"3bk3/4p3/8/1p6/1P6/8/4P3/3BK3 w - - 0 6", BISHOP_MOBILITY[2], BISHOP_MOBILITY[2]},
        {"3rk3/P2p4/8/8/8/8/p2P4/3RK3 w - - 0 7", ROOK_MOBILITY[2], ROOK_MOBILITY[2]},
        {"3qk3/P2pp3/8/1p6/1P6/8/p2PP3/3QK3 w - - 0 8", QUEEN_MOBILITY[4], QUEEN_MOBILITY[4]},

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
        {"4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1", Eval<>::IsoPawnScore, Eval<>::IsoPawnScore},
        {"rnbqkbnr/ppppp1pp/8/8/8/8/P1PPPPPP/RNBQKBNR w KQkq - 0 2",
         Eval<>::IsoPawnScore,
         Score{0}},
        {"rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 3",
         Score{0},
         Eval<>::IsoPawnScore},

        // backwards pawns
        {"4k3/8/3p4/2p5/2P5/1P6/8/4K3 w - - 0 4",
         Eval<>::BackwardPawnScore,
         Eval<>::BackwardPawnScore},
        {"4k3/8/8/2pp4/2P5/1P6/8/4K3 w - - 0 5", Eval<>::BackwardPawnScore, Score{0}},
        {"4k3/8/3p4/2p5/1PP5/8/8/4K3 w - - 0 6", Score{0}, Eval<>::BackwardPawnScore},
        // doubled pawns
        {"4k3/5pp1/4p3/3p4/3PP3/4P3/5PP1/4K3 w - - 0 7", Eval<>::DoubledPawnScore, Score{0}},
        {"4k3/5pp1/4p3/3pp3/3P4/4P3/5PP1/4K3 w - - 0 8", Score{0}, Eval<>::DoubledPawnScore},
        // other
        {"k7/8/8/8/8/P7/P7/K7 w KQkq - 0 10",
         Eval<>::IsoPawnScore * 2 + Eval<>::DoubledPawnScore,
         Score{0}},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPawnsScore(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, KnightsScore) {
    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {EMPTYFEN, Score{0}, Score{0}},
        {STARTFEN, Eval<>::MinorPawnShieldScore * 2, Eval<>::MinorPawnShieldScore * 2},
        // knight outposts
        {"6k1/8/2p5/4pNp1/3nP1P1/2P5/8/6K1 w - - 0 1", Eval<>::KnightOutpostScore, Score{0}},
        {"6k1/8/2p5/3Np1p1/4PnP1/2P5/8/6K1 w - - 0 2", Score{0}, Eval<>::KnightOutpostScore},
        // knight with reachable outposts
        {"6k1/8/2p5/1n2p1p1/4P1PN/2P5/8/6K1 w - - 0 3", Eval<>::ReachableOutpostScore, Score{0}},
        {"6k1/8/2p5/4p1pn/1N2P1P1/2P5/8/6K1 w - - 0 4", Score{0}, Eval<>::ReachableOutpostScore},
        // knight behind pawn
        {"6k1/8/4p3/8/8/4P3/4N3/6K1 w - - 0 5", Eval<>::MinorPawnShieldScore, Score{0}},
        {"6k1/4n3/4p3/8/8/4P3/8/6K1 w - - 0 6", Score{0}, Eval<>::MinorPawnShieldScore},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPiecesScore<KNIGHT>(fen, expectedWhite, expectedBlack);
    }
}

TEST_F(EvalTest, BishopsScore) {
    Score startScore         = (Eval<>::MinorPawnShieldScore * 2 + Eval<>::BishopPairScore +
                        Eval<>::PawnBlockingBishopScore * 8),
          hasOutpost         = Eval<>::BishopOutpostScore + Eval<>::PawnBlockingBishopScore * 2,
          noOutpost          = Eval<>::PawnBlockingBishopScore * 4,
          hasLongDiag        = Eval<>::BishopLongDiagScore + Eval<>::PawnBlockingBishopScore,
          noLongDiag         = Eval<>::PawnBlockingBishopScore * 2,
          twoPawnsDefended   = Eval<>::PawnBlockingBishopScore * 2 + Eval<>::BishopOutpostScore,
          twoPawnsOneBlocked = Eval<>::PawnBlockingBishopScore * 4,
          twoPawnsTwoBlocked = Eval<>::PawnBlockingBishopScore * 6;

    std::vector<std::tuple<std::string, Score, Score>> testCases = {
        {EMPTYFEN, Score{0}, Score{0}},
        {STARTFEN, startScore, startScore},
        // bishop outposts
        {"6k1/8/2p5/4pBp1/4P1P1/2P3b1/8/6K1 w - - 0 1", hasOutpost, noOutpost},
        {"6k1/8/2p3B1/4p1p1/4PbP1/2P5/8/6K1 w - - 0 2", noOutpost, hasOutpost},
        // bishop behind pawn
        {"6k1/8/4p3/8/8/4P3/4B3/6K1 w - - 0 3", Eval<>::MinorPawnShieldScore, Score{0}},
        {"6k1/4b3/4p3/8/8/4P3/8/6K1 w - - 0 4", Score{0}, Eval<>::MinorPawnShieldScore},
        // bishop on long diagonal
        {"6k1/6b1/8/3P4/3p4/8/6B1/6K1 w - - 0 5", hasLongDiag, hasLongDiag},
        {"6k1/6b1/8/4p3/4P3/8/6B1/6K1 w - - 0 6", noLongDiag, noLongDiag},
        // bishop pair
        {"5bk1/8/8/8/8/8/8/4BBK1 w - - 0 7", Eval<>::BishopPairScore, Score{0}},
        {"4bbk1/8/8/8/8/8/8/5BK1 w - - 0 8", Score{0}, Eval<>::BishopPairScore},
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
         Eval<>::RookFullOpenFileScore,
         Eval<>::RookFullOpenFileScore},
        {"6kr/p7/8/8/8/8/7P/RK6 w - - 0 2",
         Eval<>::RookSemiOpenFileScore,
         Eval<>::RookSemiOpenFileScore},
        {"rn5k/8/8/p7/P7/8/8/RN5K w - - 0 3",
         Eval<>::RookClosedFileScore,
         Eval<>::RookClosedFileScore},
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
        {"3qk3/2P5/1P6/B7/b7/1p6/8/3QK3 w - - 0 1", Eval<>::QueenDiscoveredAttackScore, Score{0}},
        {"3qk3/8/1P6/B7/b7/1p6/2p5/3QK3 w - - 0 2", Score{0}, Eval<>::QueenDiscoveredAttackScore},
        // rook discovered attack
        {"RNNqk3/8/8/8/8/8/8/rn1QK3 w - - 0 3", Eval<>::QueenDiscoveredAttackScore, Score{0}},
        {"RN1qk3/8/8/8/8/8/8/rnnQK3 w - - 0 4", Score{0}, Eval<>::QueenDiscoveredAttackScore},
    };

    for (const auto& [fen, expectedWhite, expectedBlack] : testCases) {
        testPiecesScore<QUEEN>(fen, expectedWhite, expectedBlack);
    }
}

Score calculateShelter(const std::vector<int>& shelterRanks,
                       const std::vector<int>& stormRanks,
                       const std::vector<int>& blockedRanks) {
    Score score{0, 0};
    for (int r : shelterRanks) score += PAWN_SHELTER_BONUS[r];
    for (int r : stormRanks) score += PAWN_STORM_PENALTY[r];
    for (int r : blockedRanks) score += BLOCKED_STORM_PENALTY[r];
    return score;
}

// TEST_F(EvalTest, KingScore) {
//     Score empty = calculateShelter({0, 0, 0}, {0, 0, 0}, {}) + KING_FILE_BONUS[FILE5] +
//                   KING_OPEN_FILE_BONUS[true][true];
//     Score start = calculateShelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {}) +
//                   KING_FILE_BONUS[FILE7] + KING_OPEN_FILE_BONUS[false][false];

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
    Score empty = calculateShelter({0, 0, 0}, {0, 0, 0}, {}) + KING_FILE_BONUS[FILE5] +
                  KING_OPEN_FILE_BONUS[true][true];
    Score start = calculateShelter({RANK2, RANK2, RANK2}, {RANK7, RANK7, RANK7}, {}) +
                  KING_FILE_BONUS[FILE5] + KING_OPEN_FILE_BONUS[false][false];
    Score blockedPawn = calculateShelter({RANK3, RANK4, RANK5}, {RANK6, RANK4}, {RANK5}) +
                        KING_FILE_BONUS[FILE1] + KING_OPEN_FILE_BONUS[false][false];
    Score semiOpenFile1 = calculateShelter({RANK2, RANK2, RANK2}, {0, 0, 0}, {}) +
                          KING_FILE_BONUS[FILE1] + KING_OPEN_FILE_BONUS[false][true];
    Score semiOpenFile2 = calculateShelter({0, 0, 0}, {RANK7, RANK7, RANK7}, {}) +
                          KING_FILE_BONUS[FILE1] + KING_OPEN_FILE_BONUS[true][false];
    Score kingOnRank2 = calculateShelter({0, 0, RANK3}, {RANK7, RANK7, RANK6}, {}) +
                        KING_FILE_BONUS[FILE2] + KING_OPEN_FILE_BONUS[false][false];
    Score attackedPawn = calculateShelter({RANK2, RANK2, 0}, {RANK7, RANK7, RANK7}, {}) +
                         KING_FILE_BONUS[FILE1] + KING_OPEN_FILE_BONUS[false][false];

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
