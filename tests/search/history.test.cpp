#include "search/history.hpp"
#include "search/history_learning.hpp"

#include <type_traits>

#include <gtest/gtest.h>

#include "support/search_test_access.hpp"

namespace search {

static_assert(!std::is_copy_constructible_v<HistoryLearning>);
static_assert(!std::is_move_constructible_v<HistoryLearning>);

TEST(HistoryTest, QuietScoresCombineBaseAndContinuation) {
    History               history;
    const Board           board;
    const OrderingContext positive{
        .side = BLACK, .previous_key = OrderingContext::make_previous_key(WHITE, PAWN, E4)};
    const OrderingContext negative{
        .side = WHITE, .previous_key = OrderingContext::make_previous_key(BLACK, BISHOP, D4)};
    const OrderingContext other_previous{
        .side = BLACK, .previous_key = OrderingContext::make_previous_key(WHITE, PAWN, D4)};
    const Move knight{G8, F6}, pawn{E2, E3};
    history.update_quiet(positive, board, knight, 9);
    history.update_quiet(negative, board, pawn, -9);
    EXPECT_EQ(history.base_quiet_score(BLACK, knight), 9);
    EXPECT_EQ(history.base_quiet_score(WHITE, pawn), -9);
    EXPECT_EQ(history.quiet_score(positive, board, knight), 18);
    EXPECT_EQ(history.quiet_score(positive, KNIGHT, knight), 18);
    EXPECT_EQ(history.quiet_score(negative, board, pawn), -18);
    EXPECT_EQ(history.quiet_score(negative, PAWN, pawn), -18);
    EXPECT_EQ(history.base_quiet_score(WHITE, knight), 0);
    EXPECT_EQ(history.quiet_score(other_previous, board, knight), 9);

    history.update_quiet(positive, board, knight, History::QuietMaxScore);
    history.update_quiet(negative, board, pawn, -History::QuietMaxScore);
    EXPECT_EQ(history.base_quiet_score(BLACK, knight), History::QuietMaxScore);
    EXPECT_EQ(history.base_quiet_score(WHITE, pawn), -History::QuietMaxScore);
    EXPECT_EQ(history.quiet_score(positive, board, knight), 2 * History::QuietMaxScore);
    EXPECT_EQ(history.quiet_score(negative, board, pawn), -2 * History::QuietMaxScore);
}

TEST(HistoryTest, CaptureScoresPreserveGravityAndBounds) {
    History     history;
    const Board white{"7k/8/8/4p3/8/3N4/8/7K w - - 0 1"};
    const Board black{"r6k/8/8/8/8/Q7/8/7K b - - 0 1"};
    const Move  white_move{D3, E5}, black_move{A8, A3};
    for (int i = 0; i < 3; ++i) {
        history.update_capture(white, white_move, 256);
        history.update_capture(black, black_move, -256);
    }
    EXPECT_EQ(history.capture_score(white, white_move), 757);
    EXPECT_EQ(history.capture_score(WHITE, KNIGHT, E5, PAWN), 757);
    EXPECT_EQ(history.capture_score(black, black_move), -757);
    EXPECT_EQ(history.capture_score(BLACK, ROOK, A3, QUEEN), -757);
    EXPECT_EQ(SearchTestAccess::capture_history_entry(history, WHITE, KNIGHT, E5, BISHOP), 0);
    history.update_capture(white, white_move, History::CaptureMaxScore);
    history.update_capture(black, black_move, -History::CaptureMaxScore);
    EXPECT_EQ(history.capture_score(white, white_move), History::CaptureMaxScore);
    EXPECT_EQ(history.capture_score(black, black_move), -History::CaptureMaxScore);
}

TEST(HistoryLearningTest, PreservesDepthBonusesAndQuietPenaltyDivision) {
    History               history;
    const Board           board{"7k/8/8/b1p1b3/Q7/3N4/6P1/7K w - - 0 1"};
    const OrderingContext context = history.context_for(board, 0);
    Instrumentation<>     stats;
    HistoryLearning       learning(board, history, context, 40, stats);
    const Move            king{H1, G1}, pawn{G2, G3}, capture{D3, C5}, cutoff{A4, B4};
    learning.record_non_cutoff(king, false, false);
    learning.record_non_cutoff(pawn, false, false);
    learning.record_non_cutoff(capture, true, false);
    learning.learn_from_cutoff(cutoff, false);

    EXPECT_EQ(history.base_quiet_score(WHITE, cutoff), 1024);
    // Divide depth squared before clamping: 1600 / 2 is 800, not 1024 / 2.
    EXPECT_EQ(history.base_quiet_score(WHITE, king), -800);
    EXPECT_EQ(history.base_quiet_score(WHITE, pawn), -800);
    EXPECT_EQ(history.capture_score(board, capture), -2048);

    history.clear();
    stats.reset();
    HistoryLearning bounded(board, history, context, 4, stats);
    for (int i = 0; i < 32; ++i) {
        bounded.record_non_cutoff(king, false, false);
        bounded.record_non_cutoff(capture, true, false);
    }
    const Move extra_capture{D3, E5};
    bounded.record_non_cutoff(pawn, false, false);
    bounded.record_non_cutoff(extra_capture, true, false);
    bounded.learn_from_cutoff(cutoff, false);
    EXPECT_LT(history.base_quiet_score(WHITE, king), 0);
    EXPECT_LT(history.capture_score(board, capture), 0);
    EXPECT_EQ(history.base_quiet_score(WHITE, pawn), 0);
    EXPECT_EQ(history.capture_score(board, extra_capture), 0);
#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(stats.raw_counters().quiet_penalty_candidates, 32U);
    EXPECT_EQ(stats.raw_counters().quiet_penalty_updates, 32U);
#endif
}

TEST(HistoryTest, QuietRefutationsRotateAndReplace) {
    History               history;
    const Board           board;
    const OrderingContext first_context{
        .side = BLACK, .previous_key = OrderingContext::make_previous_key(WHITE, PAWN, E4)};
    const OrderingContext second_context{
        .side = BLACK, .previous_key = OrderingContext::make_previous_key(WHITE, PAWN, D4)};
    const Move first{C2, C4}, second{D2, D4}, third{E2, E4}, other_ply{G2, G3};
    history.update_quiet_refutations(first_context, first);
    EXPECT_EQ(history.context_for(board, 0).primary_killer, first);
    EXPECT_EQ(history.context_for(board, 0).secondary_killer, NULL_MOVE);
    history.update_quiet_refutations(second_context, second);
    EXPECT_EQ(SearchTestAccess::counter_move_entry(history, first_context), first);
    EXPECT_EQ(SearchTestAccess::counter_move_entry(history, second_context), second);
    history.update_quiet_refutations(first_context, third);
    EXPECT_EQ(history.context_for(board, 0).primary_killer, third);
    EXPECT_EQ(history.context_for(board, 0).secondary_killer, second);
    EXPECT_FALSE(history.is_killer(first, 0));
    EXPECT_EQ(SearchTestAccess::counter_move_entry(history, first_context), third);
    EXPECT_EQ(SearchTestAccess::counter_move_entry(history, second_context), second);

    // An unchanged primary killer must still replace another counter key.
    history.update_quiet_refutations(second_context, third);
    EXPECT_EQ(history.context_for(board, 0).secondary_killer, second);
    EXPECT_EQ(SearchTestAccess::counter_move_entry(history, second_context), third);
    auto other_context = first_context;
    other_context.ply  = 1;
    history.update_quiet_refutations(other_context, other_ply);
    EXPECT_EQ(history.context_for(board, 1).primary_killer, other_ply);
    EXPECT_EQ(history.context_for(board, 0).primary_killer, third);
}

} // namespace search
