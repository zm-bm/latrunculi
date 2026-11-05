#include "search/ordering/picker.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "movegen/generator_detail.hpp"
#include "search/ordering/history.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_test_access.hpp"
#include "support/see_oracle.hpp"

namespace search::ordering {

class PickerTest : public ::testing::Test {
protected:
    int     ply = 5;
    History history;
};

namespace {

constexpr std::string_view WEAK_CAPTURE_FEN          = "2b3k1/3p4/8/8/8/8/8/3Q2K1 w - - 0 1";
constexpr std::string_view PROMOTION_AND_CAPTURE_FEN = "4k3/P7/8/1p6/3N4/8/8/4K3 w - - 0 1";

std::vector<Move> collect_moves(auto& picker) {
    std::vector<Move> moves;
    moves.reserve(movegen::MoveList::capacity);
    for (Move move = picker.next(); !move.is_null(); move = picker.next())
        moves.push_back(move);
    return moves;
}

std::vector<Move> picked_main_search(const Board& board,
                                     History&     history,
                                     int          ply,
                                     Move         tt_move = NULL_MOVE,
                                     Move         counter = NULL_MOVE) {
    const auto context = NodeContext::from_board(board);
    if (!counter.is_null() && context.has_previous_move())
        SearchTestAccess::counter_move_entry(history, context) = counter;
    auto picker = MainPicker(board, history, context, ply, tt_move);
    return collect_moves(picker);
}

std::vector<Move> picked_qsearch(const Board& board, History& history, Move tt_move = NULL_MOVE) {
    auto picker = QuiescencePicker(board, history, tt_move);
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

TEST_F(PickerTest, ReturnsLegalMainAndQuiescenceMoves) {
    constexpr std::string_view positions[]{board_test::fen::start,
                                           board_test::fen::perft_position_2,
                                           board_test::fen::legal_en_passant_a3,
                                           "1rr4k/P7/8/8/8/8/8/K7 w - - 0 1",
                                           WEAK_CAPTURE_FEN};
    for (std::string_view fen : positions) {
        SCOPED_TRACE(fen);
        Board position{fen};
        ASSERT_FALSE(position.is_check());

        movegen::MoveList expected_main, expected_qsearch;
        for (Move move : movegen::detail::generate_pseudo_legal(position)) {
            if (!position.is_legal_pseudo_move(move))
                continue;
            expected_main.add(move);
            if (move.type() == MOVE_PROM
                || (position.is_capture(move) && board_test::exchange_oracle(position, move) >= 0))
                expected_qsearch.add(move);
        }
        EXPECT_TRUE(
            std::ranges::is_permutation(picked_main_search(position, history, ply), expected_main));
        EXPECT_TRUE(
            std::ranges::is_permutation(picked_qsearch(position, history), expected_qsearch));
    }
}

TEST_F(PickerTest, MainSearchOrdersStagesAndDeduplicatesHints) {
    Board position{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 b - - 0 1"};
    position.make(Move(D6, D5));
    const Move tt_move{E2, E3}, capture{B4, F4};
    const Move killer_1{A5, A4}, killer_2{A5, A6}, counter{B4, D4};
    const Move history_move{B4, C4}, continuation_move{B4, B3};
    const auto context = NodeContext::from_board(position);

    history.update_quiet_refutations(NodeContext{}, killer_2, ply);
    history.update_quiet_refutations(NodeContext{}, killer_1, ply);
    SearchTestAccess::quiet_history_entry(history, WHITE, history_move.from(), history_move.to()) =
        History::QuietMaxScore;
    SearchTestAccess::continuation_history_entry(history,
                                                 context,
                                                 position.piece_type_on(continuation_move.from()),
                                                 continuation_move.to()) = 16;

    const auto              moves = picked_main_search(position, history, ply, tt_move, counter);
    const std::vector<Move> prefix{
        tt_move, capture, killer_1, killer_2, counter, history_move, continuation_move};
    ASSERT_GE(moves.size(), prefix.size());
    EXPECT_EQ(std::vector<Move>(moves.begin(), moves.begin() + prefix.size()), prefix);

    const auto duplicated = picked_main_search(position, history, ply, killer_1, killer_1);
    expect_hash_move_first_once(duplicated, killer_1);
    EXPECT_TRUE(std::ranges::is_permutation(duplicated, moves));
}

TEST_F(PickerTest, EvasionsUseMaterialOrderingAndLegalTtHints) {
    Board position{"1r5k/8/8/b7/8/P7/2N5/1Q2K3 b - - 0 1"};
    position.make(Move(A5, B4));
    ASSERT_TRUE(position.is_check());
    const Move knight_capture{C2, B4}, pawn_capture{A3, B4}, queen_capture{B1, B4};
    const Move invalid_hint{B1, B2}, tt_evasion{E1, F1};
    const auto baseline = picked_main_search(position, history, ply);
    ASSERT_GE(baseline.size(), 3U);
    EXPECT_EQ(baseline[0], pawn_capture);
    EXPECT_EQ(baseline[1], knight_capture);
    EXPECT_EQ(baseline[2], queen_capture);
    expect_capture_before_quiets(position, baseline, queen_capture, true);
    movegen::MoveList expected;
    for (Move move : movegen::detail::generate_pseudo_legal(position)) {
        if (position.is_legal_pseudo_move(move))
            expected.add(move);
    }
    EXPECT_TRUE(std::ranges::is_permutation(baseline, expected));

    const auto last_quiet = std::find_if(
        baseline.rbegin(), baseline.rend(), [&](Move move) { return !position.is_capture(move); });
    ASSERT_NE(last_quiet, baseline.rend());
    SearchTestAccess::continuation_history_entry(history,
                                                 NodeContext::from_board(position),
                                                 position.piece_type_on(last_quiet->from()),
                                                 last_quiet->to()) = 16;
    history.update_quiet_refutations(NodeContext{}, invalid_hint, ply);
    history.update_capture(position, queen_capture, 2048);
    ASSERT_TRUE(position.is_pseudo_legal(invalid_hint));
    ASSERT_FALSE(position.is_legal_pseudo_move(invalid_hint));
    EXPECT_EQ(picked_main_search(position, history, ply, invalid_hint, invalid_hint), baseline);
    EXPECT_EQ(picked_qsearch(position, history, invalid_hint), baseline);
    expect_hash_move_first_once(picked_main_search(position, history, ply, tt_evasion), tt_evasion);
    expect_hash_move_first_once(picked_qsearch(position, history, tt_evasion), tt_evasion);

    auto picker = MainPicker(position, history, NodeContext::from_board(position), ply);
    picker.skip_nonchecking_quiets();
    EXPECT_EQ(collect_moves(picker), baseline);
}

TEST_F(PickerTest, PromotionsPrecedeCaptures) {
    const Board position{PROMOTION_AND_CAPTURE_FEN};
    const Move  promotion{A7, A8, MOVE_PROM, QUEEN}, capture{D4, B5};
    const auto  moves        = picked_main_search(position, history, ply);
    const auto  promotion_it = std::find(moves.begin(), moves.end(), promotion);
    const auto  capture_it   = std::find(moves.begin(), moves.end(), capture);
    ASSERT_NE(promotion_it, moves.end());
    ASSERT_NE(capture_it, moves.end());
    EXPECT_LT(promotion_it, capture_it);
}

TEST_F(PickerTest, ValidatesAndPrioritizesHints) {
    Board position{"2b1k3/P2p4/8/8/8/8/6P1/3Q2K1 b - - 0 1"};
    position.make(Move(E8, F8));
    const Move capture{D1, D7}, promotion{A7, A8, MOVE_PROM, QUEEN};
    ASSERT_FALSE(position.see(capture, 0));
    history.update_capture(position, capture, -History::CaptureMaxScore);
    for (Move hint : {capture, promotion}) {
        SCOPED_TRACE(hint.str());
        expect_hash_move_first_once(picked_main_search(position, history, ply, hint), hint);
        expect_hash_move_first_once(picked_qsearch(position, history, hint), hint);
    }
    EXPECT_EQ(picked_qsearch(position, history, Move(D1, D2)), picked_qsearch(position, history));

    const auto context  = NodeContext::from_board(position);
    const auto baseline = picked_main_search(position, history, ply);
    for (Move hint : {Move(A1, A3), capture, promotion}) {
        SCOPED_TRACE(hint.str());
        SearchTestAccess::counter_move_entry(history, context) = hint;
        auto picker = MainPicker(position, history, context, ply);
        SearchTestAccess::counter_move_entry(history, context) = NULL_MOVE;
        EXPECT_EQ(picker.counter_hint(), hint);
        EXPECT_EQ(collect_moves(picker), baseline);
    }
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

    history.update_quiet_refutations(NodeContext{}, checking, ply);
    history.update_quiet_refutations(NodeContext{}, nonchecking, ply);
    const auto context = NodeContext::from_board(position);
    auto       picker  = MainPicker(position, history, context, ply);
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

TEST_F(PickerTest, CaptureOrderingCombinesMaterialAndHistory) {
    const Board             position{"7k/8/8/b1p1b3/Q7/3N4/6P1/7K w - - 0 1"};
    const Move              knight_bishop{D3, E5}, queen_bishop{A4, A5}, knight_pawn{D3, C5};
    const std::vector<Move> expected{knight_bishop, queen_bishop, knight_pawn};
    const auto              main_moves = picked_main_search(position, history, ply);
    ASSERT_GE(main_moves.size(), expected.size());
    EXPECT_EQ(std::vector<Move>(main_moves.begin(), main_moves.begin() + expected.size()),
              expected);
    EXPECT_EQ(picked_qsearch(position, history), expected);
    history.update_capture(position, knight_pawn, History::CaptureMaxScore);
    EXPECT_EQ(picked_main_search(position, history, ply).front(), knight_pawn);
    EXPECT_EQ(picked_qsearch(position, history).front(), knight_pawn);
}

TEST_F(PickerTest, CaptureOrderingKeepsScoringTimeHistorySnapshot) {
    const Board position{"7k/8/8/2p1p3/8/3N4/5pP1/K7 w - - 0 1"};
    const auto  frozen = picked_main_search(position, history, ply);
    ASSERT_GE(frozen.size(), 3U);
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(position.is_capture(frozen[i]));
    auto picker = MainPicker(position, history, NodeContext::from_board(position), ply);
    ASSERT_EQ(picker.next(), frozen.front());
    history.update_capture(position, frozen[1], -History::CaptureMaxScore);
    history.update_capture(position, frozen[2], History::CaptureMaxScore);
    EXPECT_EQ(collect_moves(picker), std::vector<Move>(frozen.begin() + 1, frozen.end()));
    EXPECT_EQ(picked_main_search(position, history, ply).front(), frozen[2]);
}

TEST_F(PickerTest, CaptureStagingKeepsScoringTimeHistory) {
    const Board position{"3r4/7k/q7/3r4/3QP3/8/R7/1K6 w - - 0 1"};
    const Move  first{A2, A6}, second{D4, D5};
    auto picker = MainPicker(position, history, NodeContext::from_board(position), ply, NULL_MOVE);
    ASSERT_EQ(picker.next(), first);
    history.update_capture(position, second, -History::CaptureMaxScore);
    expect_capture_before_quiets(position, collect_moves(picker), second, true);
    expect_capture_before_quiets(
        position, picked_main_search(position, history, ply), second, false);
}

TEST_F(PickerTest, MainCaptureStagingUsesHistoryWhileQuiescenceUsesZeroSee) {
    const Board position{"7k/8/5p2/4n3/3B4/8/8/K7 w - - 0 1"};
    const Move  capture{D4, E5};
    ASSERT_LT(board_test::exchange_oracle(position, capture), 0);
    for (int history_sign : {-1, 0, 1}) {
        SCOPED_TRACE(history_sign);
        history.clear();
        history.update_capture(position, capture, history_sign * History::CaptureMaxScore);
        expect_capture_before_quiets(
            position, picked_main_search(position, history, ply), capture, history_sign >= 0);
        EXPECT_TRUE(picked_qsearch(position, history).empty());
    }
}

TEST_F(PickerTest, DeferredCapturesFollowQuietsAndKeepSwapOrder) {
    const Board position{"3r1r2/7k/3r4/8/3Q1b2/8/p7/K7 w - - 0 1"};
    const Move  early{A1, A2}, rook{D4, D6}, bishop{D4, F4};
    ASSERT_FALSE(position.is_check());
    const auto moves = picked_main_search(position, history, ply);
    ASSERT_GT(moves.size(), 3U);
    EXPECT_EQ(moves.front(), early);
    for (auto it = moves.begin() + 1; it != moves.end() - 2; ++it)
        EXPECT_FALSE(position.is_capture(*it));
    // Both queen captures fail SEE. Taking the king capture swaps the rook
    // capture behind the bishop capture; deferred moves keep that order, not MVV.
    EXPECT_EQ(std::vector<Move>(moves.end() - 2, moves.end()), (std::vector<Move>{bishop, rook}));
    EXPECT_EQ(picked_qsearch(position, history), (std::vector<Move>{early}));

    // Emitting the king capture as a TT hint excludes its slot without a selection swap.
    const auto hinted = picked_main_search(position, history, ply, early);
    ASSERT_EQ(hinted.size(), moves.size());
    EXPECT_EQ(hinted.front(), early);
    EXPECT_EQ(std::vector<Move>(hinted.end() - 2, hinted.end()), (std::vector<Move>{rook, bishop}));
}

} // namespace search::ordering
