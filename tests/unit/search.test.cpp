
#include <gtest/gtest.h>

#include "constants.hpp"
#include "engine.hpp"
#include "thread.hpp"

std::ostringstream oss;

class SearchTest : public ::testing::Test {
   private:
    Context context{false, 10, 2000};
    Engine engine{std::cout, std::cin};
    Thread thread{0, &engine};

   protected:
    void testSearch(const std::string& fen, int expectedScore, Move expectedMove) {
        ThreadPool::stopSignal = false;
        context.startTime      = std::chrono::high_resolution_clock::now();
        thread.set(fen, context);

        EXPECT_EQ(thread.search(), expectedScore) << fen;
        if (expectedMove != NullMove) EXPECT_EQ(thread.pv.bestMove(), expectedMove) << fen;
    }

    void testSearchGT(const std::string& fen, int expectedScore, Move expectedMove) {
        ThreadPool::stopSignal = false;
        context.startTime      = std::chrono::high_resolution_clock::now();
        thread.set(fen, context);

        EXPECT_GT(thread.search(), expectedScore) << fen;
        if (expectedMove != NullMove) EXPECT_EQ(thread.pv.bestMove(), expectedMove) << fen;
    }
};

TEST_F(SearchTest, basicMates) {
    auto searchpos1 = "7R/8/8/8/8/1K6/8/1k6 w - -";
    auto searchpos2 = "5rk1/pb2npp1/1pq4p/5p2/5B2/1B6/P2RQ1PP/2r1R2K b - -";
    auto searchpos3 = "5rk1/pb2npp1/1p5p/5p2/5B2/1B6/P2RQ1qP/2r1R2K w - -";
    auto searchpos4 = "5rk1/pb2npp1/1p5p/5p2/5B2/1B6/P2R2QP/2r1R2K b - -";

    std::vector<std::tuple<std::string, int, Move>> testCases = {
        {searchpos1, +(MATE_SCORE - 1), Move(H8, H1)},
        {searchpos2, +(MATE_SCORE - 3), Move(C6, G2)},
        {searchpos3, -(MATE_SCORE - 2), Move(E2, G2)},
        {searchpos4, +(MATE_SCORE - 1), Move(C1, E1)},
    };

    for (auto& [fen, expectedScore, expectedMove] : testCases) {
        testSearch(fen, expectedScore, expectedMove);
    }
}

TEST_F(SearchTest, basicDraws) {
    auto searchpos1 = "r7/5kPK/7P/8/8/8/8/8 b - -";
    auto searchpos2 = "1r6/5kPK/7P/8/8/8/8/8 w - -";
    auto searchpos3 = "1r4Q1/5k1K/7P/8/8/8/8/8 b - -";

    std::vector<std::tuple<std::string, int, Move>> testCases = {
        {searchpos1, DRAW_SCORE, NullMove},
        {searchpos2, DRAW_SCORE, Move(G7, G8, PROMOTION, QUEEN)},
        {searchpos3, DRAW_SCORE, Move(B8, G8)},
    };

    for (auto& [fen, expectedScore, expectedMove] : testCases) {
        testSearch(fen, expectedScore, expectedMove);
    }
}

TEST_F(SearchTest, basicTactics) {
    auto searchpos1 = "k7/4r3/8/8/8/3Q4/4p3/K7 w - -";
    auto searchpos2 = "3r4/pbb1qBk1/2p4p/1p2N1p1/3r4/P3Q2P/1P3PP1/2RR2K1 w - -";

    std::vector<std::tuple<std::string, int, Move>> testCases = {
        {searchpos1, pieceValue(ROOK), Move(D3, D8)},
        {searchpos2, pieceValue(PAWN), Move(D1, D4)},
    };

    for (auto& [fen, expectedScore, expectedMove] : testCases) {
        testSearchGT(fen, expectedScore, expectedMove);
    }
}
