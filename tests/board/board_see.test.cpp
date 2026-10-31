#include "board/board.hpp"

#include <limits>
#include <string_view>

#include <gtest/gtest.h>

#include "board/see.hpp"
#include "movegen/generator.hpp"
#include "support/board_fixtures.hpp"
#include "support/see_oracle.hpp"

namespace {

void expect_threshold_see(const Board& board, Move move) {
    const int exact = board_test::exchange_oracle(board, move, see::piece_values);
    for (int threshold : {std::numeric_limits<int>::min(),
                          -1,
                          0,
                          1,
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
             // Full exchange value after successive rook recaptures.
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
        for (Move move : movegen::generate_pseudo_legal(board)) {
            if (!board.is_capture(move) && move.type() != MOVE_PROM)
                continue;
            SCOPED_TRACE(move.str());
            expect_threshold_see(board, move);
        }
    }
}
