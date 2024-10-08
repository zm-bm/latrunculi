#include "chess.hpp"

#include <gtest/gtest.h>

class ChessTest : public ::testing::Test {
   protected:
    void SetUp() override {
        Magics::init();
        chess = new Chess(G::STARTFEN);
    }

    void TearDown() override {
        delete chess;
    }

    Chess *chess;
};

TEST_F(ChessTest, ChessToFEN) {
    EXPECT_EQ(chess->toFEN(), G::STARTFEN);
    Chess* c;

    for (auto fen : G::FENS) {
        c = new Chess(fen);
        EXPECT_EQ(c->toFEN(), fen);
        delete c;
    }

}
