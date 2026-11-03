#include "search/ordering/history.hpp"

#include <gtest/gtest.h>

#include "support/search_test_access.hpp"

namespace search::ordering {

TEST(HistoryTest, QuietScoresCombineBaseAndContinuation) {
    History              history;
    const Board          board;
    const NodeContext    positive{BLACK, NodeContext::previous_index(WHITE, PAWN, E4)};
    const NodeContext    negative{WHITE, NodeContext::previous_index(BLACK, BISHOP, D4)};
    const NodeContext    other_previous{BLACK, NodeContext::previous_index(WHITE, PAWN, D4)};
    const Move           knight{G8, F6}, pawn{E2, E3};
    history.update_quiet(positive, board, knight, 9);
    history.update_quiet(negative, board, pawn, -9);
    EXPECT_EQ(history.base_quiet_score(BLACK, knight), 9);
    EXPECT_EQ(history.base_quiet_score(WHITE, pawn), -9);
    EXPECT_EQ(history.quiet_score(positive, board, knight), 18);
    EXPECT_EQ(history.quiet_score(negative, board, pawn), -18);
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
    EXPECT_EQ(history.capture_history_score(white, white_move), 757);
    EXPECT_EQ(history.capture_history_score(black, black_move), -757);
    EXPECT_EQ(SearchTestAccess::capture_history_entry(history, WHITE, KNIGHT, E5, BISHOP), 0);
    history.update_capture(white, white_move, History::CaptureMaxScore);
    history.update_capture(black, black_move, -History::CaptureMaxScore);
    EXPECT_EQ(history.capture_history_score(white, white_move), History::CaptureMaxScore);
    EXPECT_EQ(history.capture_history_score(black, black_move), -History::CaptureMaxScore);
}

TEST(NodeLearningTest, PreservesDepthBonusesAndQuietPenaltyDivision) {
    History              history;
    const Board          board{"7k/8/8/b1p1b3/Q7/3N4/6P1/7K w - - 0 1"};
    const NodeContext    context = NodeContext::from_board(board);
    Instrumentation<>    stats;
    NodeLearning         learning(board, history, context, 0, 40, NULL_MOVE, stats);
    const Move           king{H1, G1}, pawn{G2, G3}, capture{D3, C5}, cutoff{A4, B4};
    learning.record_non_cutoff(king, false, false);
    learning.record_non_cutoff(pawn, false, false);
    learning.record_non_cutoff(capture, true, false);
    learning.record_cutoff(cutoff, false);

    EXPECT_EQ(history.base_quiet_score(WHITE, cutoff), 1024);
    // Divide depth squared before clamping: 1600 / 2 is 800, not 1024 / 2.
    EXPECT_EQ(history.base_quiet_score(WHITE, king), -800);
    EXPECT_EQ(history.base_quiet_score(WHITE, pawn), -800);
    EXPECT_EQ(history.capture_history_score(board, capture), -2048);
}

TEST(HistoryTest, QuietRefutationsRotateAndReplace) {
    History              history;
    const NodeContext    first_key{BLACK, NodeContext::previous_index(WHITE, PAWN, E4)};
    const NodeContext    second_key{BLACK, NodeContext::previous_index(WHITE, PAWN, D4)};
    const Move           first{C2, C4}, second{D2, D4}, third{E2, E4}, other_ply{G2, G3};
    history.update_quiet_refutations(first_key, first, 0);
    EXPECT_EQ(history.primary_killer(0), first);
    EXPECT_EQ(history.secondary_killer(0), NULL_MOVE);
    history.update_quiet_refutations(second_key, second, 0);
    EXPECT_EQ(history.counter_hint(first_key), first);
    EXPECT_EQ(history.counter_hint(second_key), second);
    history.update_quiet_refutations(first_key, third, 0);
    EXPECT_EQ(history.primary_killer(0), third);
    EXPECT_EQ(history.secondary_killer(0), second);
    EXPECT_FALSE(history.is_killer(first, 0));
    EXPECT_EQ(history.counter_hint(first_key), third);
    EXPECT_EQ(history.counter_hint(second_key), second);

    // An unchanged primary killer must still replace another counter key.
    history.update_quiet_refutations(second_key, third, 0);
    EXPECT_EQ(history.secondary_killer(0), second);
    EXPECT_EQ(history.counter_hint(second_key), third);
    history.update_quiet_refutations(first_key, other_ply, 1);
    EXPECT_EQ(history.primary_killer(1), other_ply);
    EXPECT_EQ(history.primary_killer(0), third);
}

} // namespace search::ordering
