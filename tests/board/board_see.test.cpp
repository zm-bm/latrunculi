#include "board/board.hpp"

#include <limits>
#include <string_view>

#include <gtest/gtest.h>

#include "movegen/generator.hpp"
#include "support/board_fixtures.hpp"
#include "support/see_oracle.hpp"

namespace {

void expect_threshold_see(const Board& board, Move move) {
    const int exact = board_test::exchange_oracle(board, move);
    for (int threshold :
         {std::numeric_limits<int>::min(),
          std::numeric_limits<int>::max(),
          exact - 1,
          exact,
          exact + 1}) {
        EXPECT_EQ(board.see(move, threshold), exact >= threshold) << threshold;
    }
}

} // namespace

TEST(BoardSeeTest, ThresholdMatchesRecursiveOracleForSpecialPositions) {
    for (std::string_view fen : {
             board_test::fen::perft_position_2,
             board_test::fen::pinned_en_passant_e3,
             // A pawn is selected before a knight when both can recapture.
             "7k/1B1p4/4p3/P2p4/2P5/2n5/8/K7 w - - 0 1",
             // En passant removes a pawn shielding a rook on the target file.
             "3r3k/8/8/3pP3/8/8/8/K2R4 w - d6 0 1",
             // A pawn exchange is exactly zero, even with a recapturing king.
             "8/8/4k3/3p4/2P5/8/8/K7 w - - 0 1",
             // King recaptures are allowed only onto unattacked squares.
             "8/8/4k3/3p4/3Q4/8/8/K7 w - - 0 1",
             "8/8/4k3/3p4/3Q4/8/8/K2R4 w - - 0 1",
             // Include king captures and pinned geometric attackers.
             "7k/8/8/8/8/3p4/3RK3/3r4 w - - 0 1",
             // Successive rook recaptures reveal later attackers.
             "3r4/7k/8/3r4/3QP3/8/8/K7 w - - 0 1",
             // A recapturing pawn on its last rank is valued as a pawn.
             "7k/8/8/8/8/8/1p6/r2Q3K w - - 0 1",
             // Quiet and capture underpromotions, both defended and undefended.
             "1r5k/P7/8/8/8/8/8/K7 w - - 0 1",
             "1rr4k/P7/8/8/8/8/8/K7 w - - 0 1",
             "k7/8/8/8/8/8/p7/1RR4K b - - 0 1",
         }) {
        const Board board{fen};
        SCOPED_TRACE(fen);
        for (Move move : board_test::pseudo_legal_moves(board)) {
            if (!board.is_capture(move) && move.type() != MOVE_PROM)
                continue;
            SCOPED_TRACE(move.str());
            expect_threshold_see(board, move);
        }
    }
}

TEST(BoardSeeTest, ExplicitPricesChangeExchangeThresholdWithoutChangingDefaults) {
    Board board("4k3/8/8/3n4/4P3/8/8/4K3 w - - 0 1");
    const Move move(E4, D5);
    ASSERT_TRUE(board.is_legal_move(move));
    auto values = see::piece_values;
    values[KNIGHT] = 375;
    const int threshold = see::piece_value(KNIGHT) + 1;
    EXPECT_FALSE(board.see(move, threshold));
    EXPECT_TRUE(board.see(move, threshold, values));
    EXPECT_FALSE(board.see(move, threshold));
}

TEST(BoardSeeTest, TunedPricesMatchIndependentOracleForCapturesAndPromotions) {
    for (const char* fen :
         {Board::start_fen,
          "r3k2r/8/3q4/2pPp3/3R4/8/P6p/4K3 w kq e6 0 1",
          "1r2k3/P7/8/8/8/8/p7/1R2K3 w - - 0 1"}) {
        Board board(fen);
        for (const auto prices :
             {see::Values{0, 100, 250, 375, 500, 900, 0},
              see::Values{0, 100, 375, 500, 900, 1600, 0},
              see::Values{0, 100, 375, 375, 900, 900, 0}}) {
            for (Move move : board_test::pseudo_legal_moves(board)) {
                if (!board.is_capture(move) && move.type() != MOVE_PROM)
                    continue;
                const int exact = board_test::exchange_oracle(board, move, prices);
                for (int threshold : {exact - 1, exact, exact + 1, 0})
                    EXPECT_EQ(board.see(move, threshold, prices), exact >= threshold) << move.str();
            }
        }
    }
}
