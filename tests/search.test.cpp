
#include <gtest/gtest.h>

#include "base.hpp"
#include "constants.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "uci.hpp"

int depth    = 10;
int movetime = 2000;
std::ostringstream oss;
constexpr auto AnyMove = "ANY";

class SearchTest : public ::testing::Test {
   private:
    UCIProtocolHandler uciHandler{std::cout, std::cerr};
    ThreadPool threadPool{1, uciHandler};
    Thread* thread;
    SearchOptions options;

   protected:
    void SetUp() override {
        thread           = threadPool.threads[0].get();
        options.depth    = depth;
        options.movetime = movetime;
    }

    void testSearch(const std::string fen, int score, std::string move) {
        options.fen = fen;
        thread->set(options, Clock::now());

        EXPECT_EQ(thread->search(), score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(thread->pv.bestMove().str(), move) << fen;
        }
    }

    void testSearchGT(const std::string fen, int score, std::string move) {
        options.fen = fen;
        thread->set(options, Clock::now());

        EXPECT_GT(thread->search(), score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(thread->pv.bestMove().str(), move) << fen;
        }
    }
};

TEST_F(SearchTest, basicMates) {
    auto searchpos1 = "7R/8/8/8/8/1K6/8/1k6 w - - 0 1";
    auto searchpos2 = "5rk1/pb2npp1/1pq4p/5p2/5B2/1B6/P2RQ1PP/2r1R2K b - - 0 2";
    auto searchpos3 = "5rk1/pb2npp1/1p5p/5p2/5B2/1B6/P2RQ1qP/2r1R2K w - - 0 3";
    auto searchpos4 = "5rk1/pb2npp1/1p5p/5p2/5B2/1B6/P2R2QP/2r1R2K b - - 0 4";

    std::vector<std::tuple<std::string, int, std::string>> testCases = {
        {searchpos1, +(MATE_SCORE - 1), "h8h1"},
        {searchpos2, +(MATE_SCORE - 3), "c6g2"},
        {searchpos3, -(MATE_SCORE - 2), "e2g2"},
        {searchpos4, +(MATE_SCORE - 1), "c1e1"},
    };

    for (auto& [fen, expectedScore, expectedMove] : testCases) {
        testSearch(fen, expectedScore, expectedMove);
    }
}

TEST_F(SearchTest, basicDraws) {
    auto searchpos1 = "r7/5kPK/7P/8/8/8/8/8 b - -";
    auto searchpos2 = "1r6/5kPK/7P/8/8/8/8/8 w - -";
    auto searchpos3 = "1r4Q1/5k1K/7P/8/8/8/8/8 b - -";

    std::vector<std::tuple<std::string, int, std::string>> testCases = {
        {searchpos1, DRAW_SCORE, AnyMove},
        {searchpos2, DRAW_SCORE, "g7g8q"},
        {searchpos3, DRAW_SCORE, "b8g8"},
    };

    for (auto& [fen, expectedScore, expectedMove] : testCases) {
        testSearch(fen, expectedScore, expectedMove);
    }
}

TEST_F(SearchTest, basicTactics) {
    auto searchpos1 = "k7/4r3/8/8/8/3Q4/4p3/K7 w - -";
    auto searchpos2 = "3r4/pbb1qBk1/2p4p/1p2N1p1/3r4/P3Q2P/1P3PP1/2RR2K1 w - -";

    std::vector<std::tuple<std::string, int, std::string>> testCases = {
        {searchpos1, pieceValue(PieceType::Rook), "d3d8"},
        {searchpos2, pieceValue(PieceType::Pawn), "d1d4"},
    };

    for (auto& [fen, expectedScore, expectedMove] : testCases) {
        testSearchGT(fen, expectedScore, expectedMove);
    }
}
