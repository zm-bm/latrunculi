#include "movegen.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "board.hpp"
#include "test_util.hpp"

// Perft positions and results
// https://www.chessprogramming.org/Perft_Results

constexpr auto depth_limit = 4;

using PerftParams = std::tuple<std::string, std::vector<uint64_t>>;

PerftParams perft_start = {
    STARTFEN,
    {
        20,
        400,
        8902,
        197281,
        4865609,
        119060324,
    },
};

PerftParams perft_pos2 = {
    POS2,
    {
        48,
        2039,
        97862,
        4085603,
        193690690,
        8031647685,
    },
};

PerftParams perft_pos3 = {
    POS3,
    {
        14,
        191,
        2812,
        43238,
        674624,
        11030083,
    },
};

PerftParams perft_pos4w = {
    POS4W,
    {
        6,
        264,
        9467,
        422333,
        15833292,
        706045033,
    },
};

PerftParams perft_pos4b = {
    POS4B,
    {
        6,
        264,
        9467,
        422333,
        15833292,
        706045033,
    },
};

PerftParams perft_pos5 = {
    POS5,
    {
        44,
        1486,
        62379,
        2103487,
        89941194,
    },
};

PerftParams perft_pos6 = {
    POS6,
    {
        46,
        2079,
        89890,
        3894594,
        164075551,
        6923051137,
    },
};

class PerftTest : public ::testing::TestWithParam<PerftParams> {};

TEST_P(PerftTest, PerftDepths) {
    auto [fen, expected] = GetParam();

    TestBoard board(fen);

    std::stringstream null_stream;

    for (int depth = 1; depth <= expected.size(); ++depth) {
        if (depth > depth_limit)
            break;

        uint64_t result = board.perft(depth, null_stream);
        EXPECT_EQ(result, expected[depth - 1]) << "Failed at depth " << depth;
    }
}

INSTANTIATE_TEST_SUITE_P(StartFenPerft, PerftTest, ::testing::Values(perft_start));
INSTANTIATE_TEST_SUITE_P(Position2Perft, PerftTest, ::testing::Values(perft_pos2));
INSTANTIATE_TEST_SUITE_P(Position3Perft, PerftTest, ::testing::Values(perft_pos3));
INSTANTIATE_TEST_SUITE_P(Position4WPerft, PerftTest, ::testing::Values(perft_pos4w));
INSTANTIATE_TEST_SUITE_P(Position4BPerft, PerftTest, ::testing::Values(perft_pos4b));
INSTANTIATE_TEST_SUITE_P(Position5Perft, PerftTest, ::testing::Values(perft_pos5));
INSTANTIATE_TEST_SUITE_P(Position6Perft, PerftTest, ::testing::Values(perft_pos6));

namespace {

constexpr std::string_view PROMOTION_FEN         = "4k3/P6p/8/8/8/8/p6P/4K3 w - - 0 1";
constexpr std::string_view CAPTURE_PROMOTION_FEN = "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1";
constexpr std::string_view CASTLE_FEN            = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
constexpr std::string_view EVASION_FEN           = "k3r3/8/8/8/8/8/8/2B1K1N1 w - - 0 1";

std::vector<uint16_t> sorted_move_bits(const MoveList& movelist) {
    std::vector<uint16_t> bits;
    bits.reserve(movelist.size());
    for (const Move& move : movelist)
        bits.push_back(move.bits);
    std::sort(bits.begin(), bits.end());
    return bits;
}

bool has_duplicates(const std::vector<uint16_t>& sorted_bits) {
    return std::adjacent_find(sorted_bits.begin(), sorted_bits.end()) != sorted_bits.end();
}

bool are_disjoint(const std::vector<uint16_t>& lhs, const std::vector<uint16_t>& rhs) {
    std::vector<uint16_t> intersection;
    std::set_intersection(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::back_inserter(intersection));
    return intersection.empty();
}

std::vector<uint16_t> sorted_union(std::vector<uint16_t> lhs, const std::vector<uint16_t>& rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    std::sort(lhs.begin(), lhs.end());
    return lhs;
}

} // namespace

TEST(MoveGenTest, NonEvasions) {
    TestBoard board{STARTFEN};
    auto      movelist = movegen::generate_non_evasions(board);
    EXPECT_EQ(movelist.size(), 20);
}

TEST(MoveGenTest, Noisy) {
    TestBoard board{POS2};
    auto      movelist = movegen::generate_noisy(board);
    EXPECT_EQ(movelist.size(), 8);
}

TEST(MoveGenTest, PseudoLegalDispatchesToNonEvasionsOrEvasions) {
    TestBoard quiet_board{STARTFEN};
    EXPECT_EQ(sorted_move_bits(movegen::generate_pseudo_legal(quiet_board)),
              sorted_move_bits(movegen::generate_non_evasions(quiet_board)));

    TestBoard evasion_board{std::string(EVASION_FEN)};
    ASSERT_TRUE(evasion_board.is_check());
    EXPECT_EQ(sorted_move_bits(movegen::generate_pseudo_legal(evasion_board)),
              sorted_move_bits(movegen::generate_evasions(evasion_board)));
}

TEST(MoveGenTest, NoisyAndQuietPartitionNonEvasions) {
    const std::vector<std::string_view> fens = {
        STARTFEN,
        POS2,
        ENPASSANT_A3,
        PROMOTION_FEN,
        CAPTURE_PROMOTION_FEN,
        CASTLE_FEN,
    };

    for (const std::string_view fen : fens) {
        TestBoard board{std::string(fen)};
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
        POS2,
        ENPASSANT_A3,
        PROMOTION_FEN,
        CAPTURE_PROMOTION_FEN,
    };

    for (const std::string_view fen : fens) {
        TestBoard board{std::string(fen)};
        ASSERT_FALSE(board.is_check()) << fen;

        for (const Move& move : movegen::generate_noisy(board)) {
            EXPECT_TRUE(move.type() == MOVE_PROM || board.is_capture(move))
                << move << " in " << fen;
        }
    }
}

TEST(MoveGenTest, QuietMovesExcludeCapturesAndPromotions) {
    const std::vector<std::string_view> fens = {
        STARTFEN,
        POS2,
        ENPASSANT_A3,
        PROMOTION_FEN,
        CAPTURE_PROMOTION_FEN,
        CASTLE_FEN,
    };

    for (const std::string_view fen : fens) {
        TestBoard board{std::string(fen)};
        ASSERT_FALSE(board.is_check()) << fen;

        for (const Move& move : movegen::generate_quiet(board)) {
            EXPECT_NE(move.type(), MOVE_PROM) << move << " in " << fen;
            EXPECT_FALSE(board.is_capture(move)) << move << " in " << fen;
        }
    }
}
