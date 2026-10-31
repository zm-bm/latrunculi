#include "search/ordering/picker.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "board/see.hpp"
#include "movegen/generator.hpp"
#include "search/limits.hpp"
#include "search/ordering/state.hpp"
#include "support/board_fixtures.hpp"
#include "support/see_oracle.hpp"

namespace search::ordering {

class PickerTest : public ::testing::Test {
protected:
    int   ply = 5;
    State state;
};

namespace {

constexpr std::string_view WEAK_CAPTURE_FEN          = "2b3k1/3p4/8/8/8/8/8/3Q2K1 w - - 0 1";
constexpr std::string_view PROMOTION_AND_CAPTURE_FEN = "4k3/P7/8/1p6/3N4/8/8/4K3 w - - 0 1";

template <typename Moves>
std::vector<MoveBits> sorted_move_bits(const Moves& moves) {
    std::vector<MoveBits> bits;
    for (Move move : moves)
        bits.push_back(move.bits);
    std::sort(bits.begin(), bits.end());
    return bits;
}

std::vector<Move> collect_moves(ordering::Picker& picker) {
    std::vector<Move> moves;
    moves.reserve(movegen::MoveList::capacity);
    for (Move move = picker.next(); !move.is_null(); move = picker.next())
        moves.push_back(move);
    return moves;
}

void boost_continuation_hint(const Board&          board,
                             const State::Context& context,
                             State&                state,
                             Move                  move) {
    ASSERT_TRUE(context.has_previous_move);
    state.continuations.reward(context.previous_side,
                               context.previous_piece,
                               context.previous_to,
                               board.piece_type_on(move.from()),
                               move.to(),
                               4);
}

std::vector<Move> picked_main_search(
    const Board& board, State& state, int ply, Move tt_move = NULL_MOVE, Move counter = NULL_MOVE) {
    const auto context = State::make_context(board);
    if (!counter.is_null() && context.has_previous_move)
        state.counters.update(
            context.previous_side, context.previous_piece, context.previous_to, counter);
    auto picker = Picker::for_main_search(board, state, context, ply, tt_move);
    return collect_moves(picker);
}

std::vector<Move> picked_qsearch(const Board& board, State& state, Move tt_move = NULL_MOVE) {
    auto picker = Picker::for_quiescence(board, state, tt_move);
    return collect_moves(picker);
}

void expect_hash_move_first_once(const std::vector<Move>& moves, Move expected) {
    ASSERT_FALSE(moves.empty());
    EXPECT_EQ(moves.front(), expected);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), expected), 1);
}

void expect_capture_before_quiets(const Board&             board,
                                  const std::vector<Move>& moves,
                                  Move                     capture,
                                  bool                     expected) {
    const auto capture_it = std::find(moves.begin(), moves.end(), capture);
    const auto quiet_it   = std::find_if(moves.begin(), moves.end(), [&](Move move) {
        return move.type() != MOVE_PROM && !board.is_capture(move);
    });
    ASSERT_NE(capture_it, moves.end());
    ASSERT_NE(quiet_it, moves.end());
    EXPECT_EQ(capture_it < quiet_it, expected);
}

} // namespace

TEST_F(PickerTest, MainSearchReturnsEveryPseudoLegalMoveOnce) {
    for (std::string_view fen : {board_test::fen::start,
                                 board_test::fen::perft_position_2,
                                 board_test::fen::legal_en_passant_a3,
                                 "1rr4k/P7/8/8/8/8/8/K7 w - - 0 1"}) {
        SCOPED_TRACE(fen);
        Board position{fen};
        ASSERT_FALSE(position.is_check());

        const auto picked    = picked_main_search(position, state, ply);
        const auto generated = movegen::generate_pseudo_legal(position);
        EXPECT_EQ(sorted_move_bits(picked), sorted_move_bits(generated));
    }
}

TEST_F(PickerTest, MainSearchOrdersStagesAndDeduplicatesHints) {
    Board position{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 b - - 0 1"};
    position.make(Move(D6, D5));
    const Move tt_move{E2, E3}, capture{B4, F4};
    const Move killer_1{A5, A4}, killer_2{A5, A6}, counter{B4, D4};
    const Move history_move{B4, C4}, continuation_move{B4, B3};
    const auto context = State::make_context(position);

    state.killers.update(killer_2, ply);
    state.killers.update(killer_1, ply);
    state.quiets.reward(WHITE, history_move.from(), history_move.to(), Limits::max_depth);
    boost_continuation_hint(position, context, state, continuation_move);
    state.continuations.reward(WHITE, PAWN, D4, PAWN, E4, Limits::max_depth);

    const auto              moves = picked_main_search(position, state, ply, tt_move, counter);
    const std::vector<Move> prefix{
        tt_move, capture, killer_1, killer_2, counter, history_move, continuation_move};
    ASSERT_GE(moves.size(), prefix.size());
    EXPECT_EQ(std::vector<Move>(moves.begin(), moves.begin() + prefix.size()), prefix);

    const auto duplicated = picked_main_search(position, state, ply, killer_1, killer_1);
    expect_hash_move_first_once(duplicated, killer_1);
    EXPECT_EQ(sorted_move_bits(duplicated),
              sorted_move_bits(movegen::generate_pseudo_legal(position)));
}

TEST_F(PickerTest, MainSearchIgnoresInvalidAndTacticalCounterHints) {
    Board position{"2b1k3/P2p4/8/8/8/8/6P1/3Q2K1 b - - 0 1"};
    position.make(Move(E8, F8));
    const auto baseline = picked_main_search(position, state, ply);
    for (Move hint : {Move(A1, A3), Move(D1, D7), Move(A7, A8, MOVE_PROM, QUEEN)}) {
        SCOPED_TRACE(hint.str());
        EXPECT_EQ(picked_main_search(position, state, ply, NULL_MOVE, hint), baseline);
    }
}

TEST_F(PickerTest, EvasionsUseNumericalSeeAndIgnoreQuietOrderingState) {
    Board position{"1r5k/8/8/b7/8/P7/2N5/1Q2K3 b - - 0 1"};
    position.make(Move(A5, B4));
    ASSERT_TRUE(position.is_check());
    const Move knight_capture{C2, B4}, pawn_capture{A3, B4}, queen_capture{B1, B4};
    const Move invalid_hint{B1, B2}, tt_evasion{E1, F1};
    const auto baseline = picked_main_search(position, state, ply);
    ASSERT_GE(baseline.size(), 3U);
    EXPECT_EQ(baseline[0], knight_capture);
    EXPECT_EQ(baseline[1], pawn_capture);
    EXPECT_EQ(sorted_move_bits(baseline), sorted_move_bits(movegen::generate_evasions(position)));

    const auto last_quiet = std::find_if(
        baseline.rbegin(), baseline.rend(), [&](Move move) { return !position.is_capture(move); });
    ASSERT_NE(last_quiet, baseline.rend());
    boost_continuation_hint(position, State::make_context(position), state, *last_quiet);
    state.killers.update(invalid_hint, ply);
    state.reward_capture(position, queen_capture, Limits::max_depth);
    ASSERT_TRUE(position.is_pseudo_legal(invalid_hint));
    ASSERT_FALSE(position.is_legal_pseudo_move(invalid_hint));
    EXPECT_EQ(picked_main_search(position, state, ply, invalid_hint, invalid_hint), baseline);
    EXPECT_EQ(picked_qsearch(position, state, invalid_hint), baseline);
    expect_hash_move_first_once(picked_main_search(position, state, ply, tt_evasion), tt_evasion);
    expect_hash_move_first_once(picked_qsearch(position, state, tt_evasion), tt_evasion);

    auto picker = Picker::for_main_search(position, state, State::make_context(position), ply);
    picker.skip_nonchecking_quiets();
    EXPECT_EQ(collect_moves(picker), baseline);
}

TEST_F(PickerTest, PromotionsPrecedeCaptures) {
    const Board position{PROMOTION_AND_CAPTURE_FEN};
    const Move  promotion{A7, A8, MOVE_PROM, QUEEN}, capture{D4, B5};
    const auto  moves        = picked_main_search(position, state, ply);
    const auto  promotion_it = std::find(moves.begin(), moves.end(), promotion);
    const auto  capture_it   = std::find(moves.begin(), moves.end(), capture);
    ASSERT_NE(promotion_it, moves.end());
    ASSERT_NE(capture_it, moves.end());
    EXPECT_LT(promotion_it, capture_it);
}

TEST_F(PickerTest, QSearchReturnsOnlyNonLosingNoisyMoves) {
    for (std::string_view fen : {std::string_view{board_test::fen::perft_position_2},
                                 std::string_view{board_test::fen::legal_en_passant_a3},
                                 std::string_view{"1rr4k/P7/8/8/8/8/8/K7 w - - 0 1"},
                                 WEAK_CAPTURE_FEN}) {
        SCOPED_TRACE(fen);
        Board position{fen};
        ASSERT_FALSE(position.is_check());

        const auto        moves = picked_qsearch(position, state);
        movegen::MoveList expected;
        for (const Move move : movegen::generate_noisy(position)) {
            if (move.type() == MOVE_PROM
                || board_test::exchange_oracle(position, move, see::piece_values) >= 0)
                expected.add(move);
        }

        EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(expected));
        auto picker = Picker::for_quiescence(position, state);
        picker.skip_nonchecking_quiets();
        EXPECT_EQ(collect_moves(picker), moves);
    }
}

TEST_F(PickerTest, TtHintsPrioritizeTacticalsAndQSearchRejectsQuiets) {
    const Board position{"2b3k1/P2p4/8/8/8/8/8/3Q2K1 w - - 0 1"};
    const Move  capture{D1, D7}, promotion{A7, A8, MOVE_PROM, QUEEN};
    ASSERT_FALSE(position.see_ge(capture, 0));
    for (int i = 0; i < 128; ++i)
        state.penalize_capture(position, capture, Limits::max_depth);
    for (Move hint : {capture, promotion}) {
        SCOPED_TRACE(hint.str());
        expect_hash_move_first_once(picked_main_search(position, state, ply, hint), hint);
        expect_hash_move_first_once(picked_qsearch(position, state, hint), hint);
    }
    EXPECT_EQ(picked_qsearch(position, state, Move(D1, D2)), picked_qsearch(position, state));
}

TEST_F(PickerTest, SkipNoncheckingQuietsKeepsQuietChecksAndWeakCaptures) {
    Board      position{WEAK_CAPTURE_FEN};
    const Move nonchecking{D1, D2};
    const Move checking{D1, G4};
    const Move weak_capture{D1, D7};
    ASSERT_TRUE(position.is_pseudo_legal(nonchecking));
    ASSERT_TRUE(position.is_pseudo_legal(checking));
    ASSERT_FALSE(position.gives_check(nonchecking));
    ASSERT_TRUE(position.gives_check(checking));

    state.killers.update(checking, ply);
    state.killers.update(nonchecking, ply);
    const auto context = State::make_context(position);
    auto       picker  = Picker::for_main_search(position, state, context, ply);
    picker.skip_nonchecking_quiets();
    const auto moves = collect_moves(picker);

    EXPECT_EQ(std::find(moves.begin(), moves.end(), nonchecking), moves.end());
    EXPECT_NE(std::find(moves.begin(), moves.end(), checking), moves.end());
    EXPECT_NE(std::find(moves.begin(), moves.end(), weak_capture), moves.end());
    for (const Move move : moves) {
        if (move.type() != MOVE_PROM && !position.is_capture(move))
            EXPECT_TRUE(position.gives_check(move)) << move.str();
    }
}

TEST_F(PickerTest, CaptureHistoryCanOutweighMaterialPreference) {
    const Board position{"7k/8/8/2p1b3/8/3N4/6P1/4K3 w - - 0 1"};
    const Move  higher_victim{D3, E5};
    const Move  lower_victim{D3, C5};
    EXPECT_EQ(picked_main_search(position, state, ply).front(), higher_victim);
    for (int i = 0; i < 6; ++i)
        state.reward_capture(position, lower_victim, 8);
    EXPECT_EQ(picked_main_search(position, state, ply).front(), lower_victim);
    EXPECT_EQ(picked_qsearch(position, state).front(), lower_victim);
}

TEST_F(PickerTest, CaptureOrderingKeepsScoringTimeHistorySnapshot) {
    const Board position{"7k/8/8/2p1p3/8/3N4/5pP1/K7 w - - 0 1"};
    const auto  frozen = picked_main_search(position, state, ply);
    ASSERT_GE(frozen.size(), 3U);
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(position.is_capture(frozen[i]));
    auto picker = Picker::for_main_search(position, state, State::make_context(position), ply);
    ASSERT_EQ(picker.next(), frozen.front());
    for (int i = 0; i < 128; ++i) {
        state.penalize_capture(position, frozen[1], Limits::max_depth);
        state.reward_capture(position, frozen[2], Limits::max_depth);
    }
    EXPECT_EQ(collect_moves(picker), std::vector<Move>(frozen.begin() + 1, frozen.end()));
    EXPECT_EQ(picked_main_search(position, state, ply).front(), frozen[2]);
}

TEST_F(PickerTest, MainSeeUsesCachedHistoryRatherThanMutatedTable) {
    const Board position{"3r4/7k/q7/3r4/3QP3/8/R7/1K6 w - - 0 1"};
    const Move  first{A2, A6}, second{D4, D5};
    auto        picker =
        Picker::for_main_search(position, state, State::make_context(position), ply, NULL_MOVE);
    ASSERT_EQ(picker.next(), first);
    for (int i = 0; i < 128; ++i)
        state.penalize_capture(position, second, Limits::max_depth);
    expect_capture_before_quiets(position, collect_moves(picker), second, true);
    expect_capture_before_quiets(position, picked_main_search(position, state, ply), second, false);
}

TEST_F(PickerTest, MainCaptureStagingUsesHistoryWhileQuiescenceUsesZeroSee) {
    for (const auto fen :
         {"7k/8/5p2/4n3/3B4/8/8/K7 w - - 0 1", "7k/8/5p2/4b3/3B4/8/8/K7 w - - 0 1"}) {
        SCOPED_TRACE(fen);
        const Board position{fen};
        const Move  capture{D4, E5};
        const int   exchange = board_test::exchange_oracle(position, capture, see::piece_values);
        for (int history_sign : {-1, 0, 1}) {
            SCOPED_TRACE(history_sign);
            state.clear();
            for (int i = 0; i < 128; ++i) {
                if (history_sign < 0)
                    state.penalize_capture(position, capture, Limits::max_depth);
                if (history_sign > 0)
                    state.reward_capture(position, capture, Limits::max_depth);
            }
            expect_capture_before_quiets(
                position, picked_main_search(position, state, ply), capture, history_sign >= 0);
            const auto qsearch_moves = picked_qsearch(position, state);
            EXPECT_EQ(std::count(qsearch_moves.begin(), qsearch_moves.end(), capture),
                      exchange >= 0 ? 1 : 0);
        }
    }
}

TEST_F(PickerTest, CaptureOrderingAccountsForAttackerAsWellAsVictim) {
    const Board position{"7k/8/8/b1n5/Q7/3N4/8/7K w - - 0 1"};
    const Move  knight{D3, C5};
    EXPECT_EQ(picked_main_search(position, state, ply).front(), knight);
    EXPECT_EQ(picked_qsearch(position, state).front(), knight);
}

TEST_F(PickerTest, DeferredCapturesFollowAllQuietMoves) {
    const Board position{"3r1r2/7k/3r4/8/3Q1b2/8/8/K7 w - - 0 1"};
    const Move  rook{D4, D6}, bishop{D4, F4};
    auto        moves = picked_main_search(position, state, ply);
    auto        ri    = std::find(moves.begin(), moves.end(), rook);
    auto        bi    = std::find(moves.begin(), moves.end(), bishop);
    ASSERT_NE(ri, moves.end());
    ASSERT_NE(bi, moves.end());
    const auto first = std::min(ri, bi);
    for (auto it = first; it != moves.end(); ++it)
        EXPECT_TRUE(position.is_capture(*it));
}

} // namespace search::ordering
