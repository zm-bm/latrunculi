#include "movegen/generator.hpp"
#include "movegen/generator_detail.hpp"

#include <algorithm>
#include <array>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "support/board_fixtures.hpp"

namespace {

template <movegen::MoveGenType Kind>
std::vector<Move> emitted_moves(const Board& board) {
    std::vector<Move> moves;
    movegen::detail::generate<Kind>(board, [&](Move move, PieceType piece) {
        EXPECT_EQ(piece, board.piece_type_on(move.from())) << move;
        moves.push_back(move);
    });
    return moves;
}

bool has_duplicates(const auto& moves) {
    for (auto it = moves.begin(); it != moves.end(); ++it)
        if (std::find(it + 1, moves.end(), *it) != moves.end())
            return true;
    return false;
}

bool are_disjoint(const std::vector<Move>& lhs, const std::vector<Move>& rhs) {
    return std::none_of(lhs.begin(), lhs.end(), [&](Move move) {
        return std::find(rhs.begin(), rhs.end(), move) != rhs.end();
    });
}

} // namespace

static_assert(!std::is_copy_constructible_v<movegen::Moves<movegen::Mode::All>>);
static_assert(!std::is_move_constructible_v<movegen::Moves<movegen::Mode::All>>);

TEST(MoveGeneratorTest, PseudoLegalDispatchesToNonEvasionsOrEvasions) {
    auto check_legal = [](const Board& board) {
        std::vector<Move> expected;
        for (Move move : board_test::pseudo_legal_moves(board))
            if (board.is_legal_pseudo_move(move))
                expected.push_back(move);
        std::vector<Move>                  actual;
        movegen::Moves<movegen::Mode::All> moves(board);
        for (Move move = moves.next(); !move.is_null(); move = moves.next())
            actual.push_back(move);
        EXPECT_EQ(actual, expected);
        EXPECT_TRUE(moves.next().is_null());
        EXPECT_EQ(movegen::Moves<movegen::Mode::All>(board).next().is_null(), expected.empty());
        movegen::Moves<movegen::Mode::All> matches(board);
        if (expected.size() > 1) {
            const std::size_t middle = expected.size() / 2 - 1;
            EXPECT_EQ(matches.find([&](Move move) { return move == expected[middle]; }),
                      expected[middle]);
            EXPECT_EQ(matches.next(), expected[middle + 1]);
        }
        EXPECT_TRUE(matches.find([](Move) { return false; }).is_null());
        EXPECT_TRUE(matches.next().is_null());
    };
    Board      quiet_board{board_test::fen::start};
    const auto quiet_moves = board_test::pseudo_legal_moves(quiet_board);
    EXPECT_EQ(emitted_moves<movegen::MoveGenType::NonEvasions>(quiet_board),
              quiet_moves);

    check_legal(quiet_board);

    Board evasion_board{board_test::fen::check_evasion};
    ASSERT_TRUE(evasion_board.is_check());
    const auto evasions = board_test::pseudo_legal_moves(evasion_board);
    EXPECT_EQ(emitted_moves<movegen::MoveGenType::Evasions>(evasion_board),
              evasions);
    check_legal(evasion_board);
    check_legal(Board{board_test::fen::stalemate});

    Board      pinned{board_test::fen::pinned_rook};
    const Move illegal{E2, F2};
    ASSERT_TRUE(pinned.is_pseudo_legal(illegal));
    ASSERT_FALSE(pinned.is_legal_pseudo_move(illegal));
    int matched = 0;
    EXPECT_TRUE(movegen::Moves<movegen::Mode::All>(pinned)
                    .find([&](Move move) {
                        if (move != illegal)
                            return false;
                        ++matched;
                        return true;
                    })
                    .is_null());
    EXPECT_EQ(matched, 1); // Matching precedes legality, but cannot return an illegal move.
}

TEST(MoveGeneratorTest, EnPassantCanInterposeAgainstSliderCheck) {
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

        const std::vector<Move> evasions = board_test::pseudo_legal_moves(board);
        EXPECT_NE(std::find(evasions.begin(), evasions.end(), test.move), evasions.end())
            << test.fen;
    }
}

TEST(MoveGeneratorTest, NoisyAndQuietMovesPartitionAndClassifyNonEvasions) {
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

        const auto non_evasions = board_test::pseudo_legal_moves(board);
        const auto noisy_moves  = emitted_moves<movegen::MoveGenType::Noisy>(board);
        const auto quiet_moves  = emitted_moves<movegen::MoveGenType::Quiet>(board);
        EXPECT_FALSE(has_duplicates(non_evasions)) << fen;
        EXPECT_FALSE(has_duplicates(noisy_moves)) << fen;
        EXPECT_FALSE(has_duplicates(quiet_moves)) << fen;
        EXPECT_TRUE(are_disjoint(noisy_moves, quiet_moves)) << fen;
        auto combined = noisy_moves;
        combined.insert(combined.end(), quiet_moves.begin(), quiet_moves.end());
        EXPECT_TRUE(std::ranges::is_permutation(non_evasions, combined)) << fen;

        for (const Move& move : noisy_moves) {
            EXPECT_TRUE(move.type() == MOVE_PROM || board.is_capture(move))
                << move << " in " << fen;
        }

        for (const Move& move : quiet_moves) {
            EXPECT_NE(move.type(), MOVE_PROM) << move << " in " << fen;
            EXPECT_FALSE(board.is_capture(move)) << move << " in " << fen;
        }
    }
}

TEST(MoveGeneratorTest, DoubleCheckEvasionsContainOnlyKingMoves) {
    Board board{"R3k3/8/8/8/8/8/4Q3/4K3 b - - 0 1"};
    ASSERT_TRUE(board.is_double_check());

    const std::vector<Move> evasions = board_test::pseudo_legal_moves(board);
    ASSERT_FALSE(evasions.empty());
    for (const Move move : evasions)
        EXPECT_EQ(move.from(), E8) << move;
}
