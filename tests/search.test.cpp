
#include <gtest/gtest.h>

#include "board.hpp"

#include "search_options.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "uci.hpp"

int                depth    = 8;
int                movetime = 2000;
std::ostringstream oss;
constexpr auto     AnyMove = "ANY";

class SearchTest : public ::testing::Test {
private:
    uci::Protocol protocol{std::cout, std::cerr};
    ThreadPool    pool{1, protocol};
    Thread*       thread;
    SearchOptions options;

protected:
    void SetUp() override {
        thread        = pool.threads[0].get();
        options       = SearchOptions();
        options.depth = depth;
        // options.movetime = movetime;
    }

    void testSearch(const std::string fen, int score, std::string move) {
        Board board{fen};
        options.board = &board;
        thread->start(options);
        thread->wait();

        EXPECT_EQ(thread->root_value, score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(thread->root_move.str(), move) << fen;
        }
    }

    void testSearchGT(const std::string fen, int score, std::string move) {
        Board board{fen};
        options.board = &board;
        thread->start(options);
        thread->wait();

        EXPECT_GT(thread->root_value, score) << fen;
        if (move != AnyMove) {
            EXPECT_EQ(thread->root_move.str(), move) << fen;
        }
    }
};

TEST_F(SearchTest, basicMates) {
    auto searchpos1 = "7R/8/8/8/8/1K6/8/1k6 w - - 0 1";
    auto searchpos2 = "5rk1/pb2npp1/1pq4p/5p2/5B2/1B6/P2RQ1PP/2r1R2K b - - 0 2";
    auto searchpos3 = "5rk1/pb2npp1/1p5p/5p2/5B2/1B6/P2RQ1qP/2r1R2K w - - 0 3";
    auto searchpos4 = "5rk1/pb2npp1/1p5p/5p2/5B2/1B6/P2R2QP/2r1R2K b - - 0 4";

    std::vector<std::tuple<std::string, int, std::string>> test_cases = {
        {searchpos1, +(MATE_VALUE - 1), "h8h1"},
        {searchpos2, +(MATE_VALUE - 3), "c6g2"},
        {searchpos3, -(MATE_VALUE - 2), "e2g2"},
        {searchpos4, +(MATE_VALUE - 1), "c1e1"},
    };

    for (auto& [fen, expectedScore, expectedMove] : test_cases) {
        testSearch(fen, expectedScore, expectedMove);
    }
}

TEST_F(SearchTest, basicDraws) {
    auto searchpos1 = "r7/5kPK/7P/8/8/8/8/8 b - -";
    auto searchpos2 = "1r6/5kPK/7P/8/8/8/8/8 w - -";
    auto searchpos3 = "1r4Q1/5k1K/7P/8/8/8/8/8 b - -";

    std::vector<std::tuple<std::string, int, std::string>> test_cases = {
        {searchpos1, DRAW_VALUE, AnyMove},
        {searchpos2, DRAW_VALUE, "g7g8q"},
        {searchpos3, DRAW_VALUE, "b8g8"},
    };

    for (auto& [fen, expectedScore, expectedMove] : test_cases) {
        testSearch(fen, expectedScore, expectedMove);
    }
}

TEST_F(SearchTest, basicTactics) {
    auto searchpos1 = "k7/4r3/8/8/8/3Q4/4p3/K7 w - -";
    auto searchpos2 = "3r4/pbb1qBk1/2p4p/1p2N1p1/3r4/P3Q2P/1P3PP1/2RR2K1 w - -";

    std::vector<std::tuple<std::string, int, std::string>> test_cases = {
        {searchpos1, eval::piece(ROOK).mg, "d3d8"},
        {searchpos2, eval::piece(PAWN).mg, "d1d4"},
    };

    for (auto& [fen, expectedScore, expectedMove] : test_cases) {
        testSearchGT(fen, expectedScore, expectedMove);
    }
}
