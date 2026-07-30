#include "movegen/movegen.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "support/board_fixtures.hpp"

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

TEST(MoveGenTest, PseudoLegalDispatchesToNonEvasionsOrEvasions) {
    Board quiet_board{board_test::fen::start};
    EXPECT_EQ(sorted_move_bits(movegen::generate_pseudo_legal(quiet_board)),
              sorted_move_bits(movegen::generate_non_evasions(quiet_board)));

    Board evasion_board{board_test::fen::check_evasion};
    ASSERT_TRUE(evasion_board.is_check());
    EXPECT_EQ(sorted_move_bits(movegen::generate_pseudo_legal(evasion_board)),
              sorted_move_bits(movegen::generate_evasions(evasion_board)));
}

TEST(MoveGenTest, EnPassantCanInterposeAgainstSliderCheck) {
    struct TestCase {
        std::string_view fen;
        Move             move;
    };

    constexpr std::array cases = {
        TestCase{"7k/2K5/8/2Ppb3/8/8/8/8 w - d6 0 1", Move(C5, D6, MOVE_EP)},
        TestCase{"8/8/8/8/2pPB3/8/2k5/7K b - d3 0 1", Move(C4, D3, MOVE_EP)},
    };

    for (const auto& test : cases) {
        Board board{test.fen};
        ASSERT_TRUE(board.is_check()) << test.fen;
        ASSERT_TRUE(board.is_legal_move(test.move)) << test.fen;

        const MoveList evasions = movegen::generate_evasions(board);
        EXPECT_NE(std::find(evasions.begin(), evasions.end(), test.move), evasions.end())
            << test.fen;
    }
}

TEST(MoveGenTest, NoisyAndQuietMovesPartitionAndClassifyNonEvasions) {
    constexpr std::array fens = {
        board_test::fen::start,
        board_test::fen::perft_position_2,
        board_test::fen::legal_en_passant_a3,
        board_test::fen::promotion_options,
        board_test::fen::capture_promotion,
        board_test::fen::castling,
    };

    for (const std::string_view fen : fens) {
        Board board{fen};
        ASSERT_FALSE(board.is_check()) << fen;

        const auto non_evasions = sorted_move_bits(movegen::generate_non_evasions(board));
        const auto noisy        = sorted_move_bits(movegen::generate_noisy(board));
        const auto quiet        = sorted_move_bits(movegen::generate_quiet(board));

        EXPECT_FALSE(has_duplicates(non_evasions)) << fen;
        EXPECT_FALSE(has_duplicates(noisy)) << fen;
        EXPECT_FALSE(has_duplicates(quiet)) << fen;
        EXPECT_TRUE(are_disjoint(noisy, quiet)) << fen;
        EXPECT_EQ(non_evasions, sorted_union(noisy, quiet)) << fen;

        for (const Move& move : movegen::generate_noisy(board)) {
            EXPECT_TRUE(move.type() == MOVE_PROM || board.is_capture(move))
                << move << " in " << fen;
        }

        for (const Move& move : movegen::generate_quiet(board)) {
            EXPECT_NE(move.type(), MOVE_PROM) << move << " in " << fen;
            EXPECT_FALSE(board.is_capture(move)) << move << " in " << fen;
        }
    }
}

TEST(MoveGenTest, DoubleCheckEvasionsContainOnlyKingMoves) {
    Board board{"R3k3/8/8/8/8/8/4Q3/4K3 b - - 0 1"};
    ASSERT_TRUE(board.is_double_check());

    const MoveList evasions = movegen::generate_evasions(board);
    ASSERT_FALSE(evasions.empty());
    for (const Move move : evasions)
        EXPECT_EQ(move.from(), E8) << move;
}
