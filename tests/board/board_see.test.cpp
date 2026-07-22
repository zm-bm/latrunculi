#include "board/board.hpp"

#include <array>
#include <string_view>

#include <gtest/gtest.h>

#include "movegen/movegen.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"

namespace {

constexpr std::string_view ARASAN20_01 =
    "r1bq1r1k/p1pnbpp1/1p2p3/6p1/3PB3/5N2/PPPQ1PPP/2KR3R w - - 0 1";
constexpr std::string_view ARASAN20_08 =
    "r1r3k1/p3bppp/2bp3Q/q2pP1P1/1p1BP3/8/PPP1B2P/2KR2R1 w - - 0 1";
constexpr std::string_view ARASAN20_16 = "8/3r4/pr1Pk1p1/8/7P/6P1/3R3K/5R2 w - - 0 1";
constexpr std::string_view ARASAN20_21 = "8/5pk1/p4npp/1pPN4/1P2p3/1P4PP/5P2/5K2 w - - 0 1";
constexpr std::string_view ARASAN20_30 = "b2rk3/r4p2/p3p3/P3Q1Np/2Pp3P/8/6P1/6K1 w - - 0 1";

void expect_see_at_least_matches_exact(const Board& board, Move move) {
    const int exact = board.see(move);

    for (int threshold : std::array{exact - 1, exact, exact + 1, -1000, -100, -1, 0, 1, 100, 1000})
        EXPECT_EQ(board.see_at_least(move, threshold), exact >= threshold)
            << move.str() << " threshold " << threshold << " exact " << exact;
}

} // namespace

TEST(BoardSeeTest, BasicExchange) {
    board_test::Harness b("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -");
    EXPECT_EQ(b.see(Move(E1, E5)), eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, TradesPieces) {
    board_test::Harness b("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
    EXPECT_EQ(b.see(Move(D3, E5)), eval::piece(PAWN).mg - eval::piece(KNIGHT).mg);
}

TEST(BoardSeeTest, PrefersPawnOverKnightRecapturer) {
    board_test::Harness b("7k/1B1p4/4p3/P2p4/2P5/2n5/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(B7, D5)), eval::piece(PAWN).mg - eval::piece(BISHOP).mg);
}

TEST(BoardSeeTest, HandlesKingRecapture) {
    board_test::Harness b("8/8/4k3/3p4/3Q4/8/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(D4, D5)), eval::piece(PAWN).mg - eval::piece(QUEEN).mg);
}

TEST(BoardSeeTest, DisallowsAttackedKingRecapture) {
    board_test::Harness b("8/8/4k3/3p4/3Q4/8/8/K2R4 w - - 0 1");
    EXPECT_EQ(b.see(Move(D4, D5)), eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, AccountsForCapturePromotion) {
    board_test::Harness b("4k2r/6P1/8/8/8/8/8/K7 w - - 0 1");
    EXPECT_EQ(b.see(Move(G7, H8, MOVE_PROM, QUEEN)),
              eval::piece(ROOK).mg + eval::piece(QUEEN).mg - eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, EnPassantUsesPawnVictim) {
    board_test::Harness b(board_test::fen::legal_en_passant_a3);
    EXPECT_EQ(b.see(Move(B4, A3, MOVE_EP)), eval::piece(PAWN).mg);
}

TEST(BoardSeeTest, ThresholdMatchesExactFixtures) {
    {
        board_test::Harness b("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -");
        expect_see_at_least_matches_exact(b, Move(E1, E5));
    }
    {
        board_test::Harness b("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
        expect_see_at_least_matches_exact(b, Move(D3, E5));
    }
    {
        board_test::Harness b("8/8/4k3/3p4/3Q4/8/8/K2R4 w - - 0 1");
        expect_see_at_least_matches_exact(b, Move(D4, D5));
    }
    {
        board_test::Harness b("4k2r/6P1/8/8/8/8/8/K7 w - - 0 1");
        expect_see_at_least_matches_exact(b, Move(G7, H8, MOVE_PROM, QUEEN));
    }
    {
        board_test::Harness b(board_test::fen::legal_en_passant_a3);
        expect_see_at_least_matches_exact(b, Move(B4, A3, MOVE_EP));
    }
}

TEST(BoardSeeTest, ThresholdMatchesExactGeneratedNoisyMoves) {
    for (std::string_view fen :
         std::array{std::string_view{board_test::fen::start},
                    std::string_view{board_test::fen::perft_position_2},
                    std::string_view{board_test::fen::perft_position_3},
                    std::string_view{board_test::fen::perft_position_4_white},
                    std::string_view{board_test::fen::perft_position_4_black},
                    std::string_view{board_test::fen::perft_position_5},
                    std::string_view{board_test::fen::perft_position_6},
                    std::string_view{board_test::fen::legal_en_passant_a3},
                    ARASAN20_01,
                    ARASAN20_08,
                    ARASAN20_16,
                    ARASAN20_21,
                    ARASAN20_30}) {
        board_test::Harness b{fen};
        SCOPED_TRACE(fen);

        const MoveList moves =
            b.is_check() ? movegen::generate_evasions(b) : movegen::generate_noisy(b);
        for (Move move : moves) {
            if (move.type() != MOVE_PROM && !b.is_capture(move))
                continue;

            expect_see_at_least_matches_exact(b, move);
        }
    }
}
