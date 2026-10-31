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
        EXPECT_EQ(board.see_ge(move, threshold), exact >= threshold) << threshold;
    }
}

} // namespace

TEST(BoardSeeTest, ValuesAnUndefendedCapture) {
    Board b("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -");
    EXPECT_EQ(b.see(Move(E1, E5)), eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, AccountsForTheRecaptureSequence) {
    Board b("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
    EXPECT_EQ(b.see(Move(D3, E5)), eval::piece(PAWN).mg - eval::piece(KNIGHT).mg);
}

TEST(BoardSeeTest, PrefersPawnOverKnightRecapturer) {
    Board b("7k/1B1p4/4p3/P2p4/2P5/2n5/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(B7, D5)), eval::piece(PAWN).mg - eval::piece(BISHOP).mg);
}

TEST(BoardSeeTest, HandlesKingRecapture) {
    Board b("8/8/4k3/3p4/3Q4/8/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(D4, D5)), eval::piece(PAWN).mg - eval::piece(QUEEN).mg);
}

TEST(BoardSeeTest, DisallowsAttackedKingRecapture) {
    Board b("8/8/4k3/3p4/3Q4/8/8/K2R4 w - - 0 1");
    EXPECT_EQ(b.see(Move(D4, D5)), eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, AccountsForCapturePromotion) {
    Board b("4k2r/6P1/8/8/8/8/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(G7, H8, MOVE_PROM, QUEEN)),
              eval::piece(ROOK).mg + eval::piece(QUEEN).mg - eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, EnPassantUsesPawnVictim) {
    Board b(board_test::fen::legal_en_passant_a3);
    EXPECT_EQ(b.see(Move(B4, A3, MOVE_EP)), eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, ThresholdMatchesRecursiveOracleForSpecialPositions) {
    for (std::string_view fen : {
             board_test::fen::perft_position_2,
             board_test::fen::pinned_en_passant_e3,
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
