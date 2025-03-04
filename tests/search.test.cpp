#include "search.hpp"

#include <gtest/gtest.h>

#include <memory>

#include "constants.hpp"
#include "search.hpp"

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results

class PerftTest : public ::testing::TestWithParam<std::tuple<std::string, std::vector<long>>> {};

TEST_P(PerftTest, PerftForMultipleDepths) {
    auto [fen, expected_results] = GetParam();
    Chess chess(fen);

    for (int depth = 1; depth <= expected_results.size(); ++depth) {
        long result = Search::perft<true, false>(depth, chess);
        EXPECT_EQ(result, expected_results[depth - 1]) << "Failed at depth " << depth;
    }
}

auto startfen = std::make_tuple(STARTFEN,
                                std::vector<long>{
                                    20, 400, 8902, 197281,
                                    // 4865609,
                                    // 119060324,
                                });
INSTANTIATE_TEST_SUITE_P(StartFenPerftTestSuite, PerftTest, ::testing::Values(startfen));

auto pos2 = std::make_tuple(POS2,
                            std::vector<long>{
                                48, 2039, 97862, 4085603,
                                // 193690690,
                                // 8031647685,
                            });
INSTANTIATE_TEST_SUITE_P(Position2PerftTestSuite, PerftTest, ::testing::Values(pos2));

auto pos3 = std::make_tuple(POS3,
                            std::vector<long>{
                                14, 191, 2812, 43238, 674624,
                                // 11030083
                            });
INSTANTIATE_TEST_SUITE_P(Position3PerftTestSuite, PerftTest, ::testing::Values(pos3));

auto pos4w = std::make_tuple(POS4W,
                             std::vector<long>{
                                 6, 264, 9467, 422333,
                                 // 15833292
                             });
INSTANTIATE_TEST_SUITE_P(Position4WPerftTestSuite, PerftTest, ::testing::Values(pos4w));

auto pos4b = std::make_tuple(POS4B,
                             std::vector<long>{
                                 6, 264, 9467, 422333,
                                 // 15833292
                             });
INSTANTIATE_TEST_SUITE_P(Position4BPerftTestSuite, PerftTest, ::testing::Values(pos4b));

auto pos5 = std::make_tuple(POS5,
                            std::vector<long>{
                                44, 1486, 62379, 2103487,
                                // 89941194
                            });
INSTANTIATE_TEST_SUITE_P(Position5PerftTestSuite, PerftTest, ::testing::Values(pos5));

// prev search tests

// enum ScoreType {
//     NONESCORE,
//     EXACT,
//     MORE,
//     LESS
// };

// struct Position {
//     std::string fen;
//     Move bestMove;
//     Move avoidMove;
//     int depth;
//     Score score;
//     ScoreType type = NONESCORE;
// };

// std::vector<Position> integration = {
//     { "7R/8/8/8/8/1K6/8/1k6 w - -",                          Move(H8, H1), Move(), 1,
//     MATESCORE-1, EXACT }, // Mate in 1 { "5rk1/pb2npp1/1pq4p/5p2/5B2/1B6/P2RQ1PP/2r1R2K b - -",
//     Move(C6, G2), Move(), 3, MATESCORE-3, EXACT }, // Mate in 2 { "k7/8/4r3/8/8/3Q4/4p3/K7 w -
//     -",                       Move(D3, D5), Move(), 4, ROOKSCORE,   MORE },
//     // Find tactical win { "R1R5/7R/1k6/7R/8/P1P5/PKP5/1RP5 w - -",               Move(B2, A1),
//     Move(), 1, MATESCORE-1, EXACT }, // Mate in 1 avoid stalemate { "R1R5/7R/1k6/7R/8/8/8/1K6 b -
//     -", Move(), Move(B6, B5), 1, DRAWSCORE,   EXACT }, // Evaluate stalemate
// };

// TEST_CASE( "Integration search tests", "[search-integration]" )
// {
//     G::init();

//     SECTION("Search tests")
//     {
//         for (auto& pos : integration)
//         {
//             auto board = Board(pos.fen);
//             Search search(&board);
//             search.think(pos.depth);

//             switch (pos.type)
//             {
//             case EXACT:
//                 REQUIRE(search.bestScore == pos.score);
//                 break;
//             case MORE:
//                 REQUIRE(search.bestScore > pos.score);

//             default:
//                 break;
//             }

//             REQUIRE(search.bestMove == pos.bestMove);
//             REQUIRE(!(search.bestMove == pos.avoidMove));
//         }
//     }
// }

// std::vector<Position> positional = {
//     { "rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/2N1P3/PP3PPP/R1BQKBNR w KQkq - 0 1",  Move(D1, B3), Move(),
//     10, -1 }, { "rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/1QN1P3/PP3PPP/R1B1KBNR b KQkq - 1 1", Move(F5,
//     C8), Move(), 10, -1 }, { "r1bqk2r/ppp2ppp/2n5/4P3/2Bp2n1/5N1P/PP1N1PP1/R2Q1RK1 b kq - 1 10",
//     Move(G4, H6), Move(G4, E5), 10, -1 }, {
//     "r1bqrnk1/pp2bp1p/2p2np1/3p2B1/3P4/2NBPN2/PPQ2PPP/1R3RK1 w - - 1 12", Move(B2, B4), Move(),
//     10, -1 }, { "rnbqkb1r/ppp1pppp/5n2/8/3PP3/2N5/PP3PPP/R1BQKBNR b KQkq - 3 5",      Move(E7,
//     E5), Move(), 10, -1 },
// { "rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ - 1 5" bm Bcx3+;
// { "r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 12"  bm Rfb8;
// { "rn1qkb1r/pb1p1ppp/1p2pn2/2p5/2PP4/5NP1/PP2PPBP/RNBQK2R w KQkq c6 1 6"  bm d5;
// { "r1bq1rk1/1pp2pbp/p1np1np1/3Pp3/2P1P3/2N1BP2/PP4PP/R1NQKB1R b KQ - 1 9"  bm Nd4;
// { "rnbqr1k1/1p3pbp/p2p1np1/2pP4/4P3/2N5/PP1NBPPP/R1BQ1RK1 w - - 1 11"  bm a4;
// { "rnbqkb1r/pppp1ppp/5n2/4p3/4PP2/2N5/PPPP2PP/R1BQKBNR b KQkq f3 1 3"  bm d5;
// { "r1bqk1nr/pppnbppp/3p4/8/2BNP3/8/PPP2PPP/RNBQK2R w KQkq - 2 6" bm Bxf7+;
// { "rnbq1b1r/ppp2kpp/3p1n2/8/3PP3/8/PPP2PPP/RNBQKB1R b KQ d3 1 5" am Ne4;
// { "rnbqkb1r/pppp1ppp/3n4/8/2BQ4/5N2/PPP2PPP/RNB2RK1 b kq - 1 6"  am Nxc4;
// { "r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 w - - 1 12"  bm exf6;
// { "r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq - 2 7"  bm d5;
// { "r2qkbnr/2p2pp1/p1pp4/4p2p/4P1b1/5N1P/PPPP1PP1/RNBQ1RK1 w kq - 1 8"  am hxg4;
// { "r1bqkb1r/pp3ppp/2np1n2/4p1B1/3NP3/2N5/PPP2PPP/R2QKB1R w KQkq e6 1 7"  bm Bxf6+;
// { "rn1qk2r/1b2bppp/p2ppn2/1p6/3NP3/1BN5/PPP2PPP/R1BQR1K1 w kq - 5 10"  am Bxe6;
// { "r1b1kb1r/1pqpnppp/p1n1p3/8/3NP3/2N1B3/PPP1BPPP/R2QK2R w KQkq - 3 8"  am Ndb5;
// { "r1bqnr2/pp1ppkbp/4N1p1/n3P3/8/2N1B3/PPP2PPP/R2QK2R b KQ - 2 11"  am Kxe6;
// { "r3kb1r/pp1n1ppp/1q2p3/n2p4/3P1Bb1/2PB1N2/PPQ2PPP/RN2K2R w KQkq - 3 11"  bm a4;
// { "r1bq1rk1/pppnnppp/4p3/3pP3/1b1P4/2NB3N/PPP2PPP/R1BQK2R w KQ - 3 7"  bm Bxh7+;
// { "r2qkbnr/ppp1pp1p/3p2p1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R w KQkq - 2 6"  bm Nxe5;
// { "rn2kb1r/pp2pppp/1qP2n2/8/6b1/1Q6/PP1PPPBP/RNB1K1NR b KQkq - 1 6"  am Qxb3;
// };

// TEST_CASE( "Positional search tests", "[search-position]" )
// {
//     G::init();

//     SECTION("Search tests")
//     {
//         for (auto& pos : positional)
//         {
//             auto board = Board(pos.fen);
//             Search search(&board);
//             search.think(pos.depth);

//             if (pos.mateDepth > 0)
//                 REQUIRE(MATESCORE - pos.mateDepth == search.bestScore);

//             REQUIRE(search.bestMove == pos.bestMove);
//             REQUIRE(!(search.bestMove == pos.avoidMove));
//         }
//     }

// }

// Prev zobrist tests

// void recursiveZobristCheck(Board board, int depth)
// {
//     if (depth < 0)
//         return;

//     auto gen  = MoveGenerator::Generator(&board);
//     gen.generatePseudoLegalMoves();

//     for (auto& move : gen.moves)
//     {
//         if (!board.isLegalMove(move))
//             continue;

//         board.make(move);
//         REQUIRE(board.calculateKey() == board.getKey());
//         recursiveZobristCheck(board, depth-1);
//         board.unmake();
//     }
// };

// TEST_CASE( "Zobrist hashing", "[zobrist]" )
// {
//     G::init();

//     SECTION("Check starting hash validity")
//     {
//         auto board = Board(STARTFEN);
//         REQUIRE(board.calculateKey() == board.getKey());
//         board = Board(G::KIWIPETE);
//         REQUIRE(board.calculateKey() == board.getKey());
//     }

//     SECTION("Check hash validity after making moves")
//     {
//         // auto board = Board(STARTFEN);
//         // recursiveZobristCheck(board, 5);
//         // board = Board(G::KIWIPETE);
//         // recursiveZobristCheck(board, 3);
//         auto board = Board(G::KIWIPETE);
//         recursiveZobristCheck(board, 2);
//     }
// }
