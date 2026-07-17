#include "movegen/movegen.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"

namespace {

std::vector<MoveBits> sorted_move_bits(const MoveList& movelist) {
    std::vector<MoveBits> bits;
    bits.reserve(movelist.size());
    for (const Move& move : movelist)
        bits.push_back(move.bits);
    std::sort(bits.begin(), bits.end());
    return bits;
}

bool has_duplicates(const std::vector<MoveBits>& sorted_bits) {
    return std::adjacent_find(sorted_bits.begin(), sorted_bits.end()) != sorted_bits.end();
}

bool are_disjoint(const std::vector<MoveBits>& lhs, const std::vector<MoveBits>& rhs) {
    std::vector<MoveBits> intersection;
    std::set_intersection(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::back_inserter(intersection));
    return intersection.empty();
}

std::vector<MoveBits> sorted_union(std::vector<MoveBits> lhs, const std::vector<MoveBits>& rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    std::sort(lhs.begin(), lhs.end());
    return lhs;
}

} // namespace

TEST(MoveGenTest, NonEvasions) {
    board_test::Harness board{board_test::fen::start};
    auto                movelist = movegen::generate_non_evasions(board);
    EXPECT_EQ(movelist.size(), 20);
}

TEST(MoveGenTest, Noisy) {
    board_test::Harness board{board_test::fen::perft_position_2};
    auto                movelist = movegen::generate_noisy(board);
    EXPECT_EQ(movelist.size(), 8);
}

TEST(MoveGenTest, PseudoLegalDispatchesToNonEvasionsOrEvasions) {
    board_test::Harness quiet_board{board_test::fen::start};
    EXPECT_EQ(sorted_move_bits(movegen::generate_pseudo_legal(quiet_board)),
              sorted_move_bits(movegen::generate_non_evasions(quiet_board)));

    board_test::Harness evasion_board{board_test::fen::check_evasion};
    ASSERT_TRUE(evasion_board.is_check());
    EXPECT_EQ(sorted_move_bits(movegen::generate_pseudo_legal(evasion_board)),
              sorted_move_bits(movegen::generate_evasions(evasion_board)));
}

TEST(MoveGenTest, NoisyAndQuietPartitionNonEvasions) {
    const std::vector<std::string_view> fens = {
        board_test::fen::start,
        board_test::fen::perft_position_2,
        board_test::fen::legal_en_passant_a3,
        board_test::fen::promotion_options,
        board_test::fen::capture_promotion,
        board_test::fen::castling,
    };

    for (const std::string_view fen : fens) {
        board_test::Harness board{fen};
        ASSERT_FALSE(board.is_check()) << fen;

        const auto non_evasions = sorted_move_bits(movegen::generate_non_evasions(board));
        const auto noisy        = sorted_move_bits(movegen::generate_noisy(board));
        const auto quiet        = sorted_move_bits(movegen::generate_quiet(board));

        EXPECT_FALSE(has_duplicates(non_evasions)) << fen;
        EXPECT_FALSE(has_duplicates(noisy)) << fen;
        EXPECT_FALSE(has_duplicates(quiet)) << fen;
        EXPECT_TRUE(are_disjoint(noisy, quiet)) << fen;
        EXPECT_EQ(non_evasions, sorted_union(noisy, quiet)) << fen;
    }
}

TEST(MoveGenTest, NoisyMovesAreCapturesEnPassantOrPromotions) {
    const std::vector<std::string_view> fens = {
        board_test::fen::perft_position_2,
        board_test::fen::legal_en_passant_a3,
        board_test::fen::promotion_options,
        board_test::fen::capture_promotion,
    };

    for (const std::string_view fen : fens) {
        board_test::Harness board{fen};
        ASSERT_FALSE(board.is_check()) << fen;

        for (const Move& move : movegen::generate_noisy(board)) {
            EXPECT_TRUE(move.type() == MOVE_PROM || board.is_capture(move))
                << move << " in " << fen;
        }
    }
}

TEST(MoveGenTest, QuietMovesExcludeCapturesAndPromotions) {
    const std::vector<std::string_view> fens = {
        board_test::fen::start,
        board_test::fen::perft_position_2,
        board_test::fen::legal_en_passant_a3,
        board_test::fen::promotion_options,
        board_test::fen::capture_promotion,
        board_test::fen::castling,
    };

    for (const std::string_view fen : fens) {
        board_test::Harness board{fen};
        ASSERT_FALSE(board.is_check()) << fen;

        for (const Move& move : movegen::generate_quiet(board)) {
            EXPECT_NE(move.type(), MOVE_PROM) << move << " in " << fen;
            EXPECT_FALSE(board.is_capture(move)) << move << " in " << fen;
        }
    }
}
