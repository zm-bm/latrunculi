#include "eval/evaluation.hpp"

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "eval/parameters.hpp"
#include "eval/trace_formatter.hpp"
#include "support/board_fixtures.hpp"

TEST(EvaluatorTest, Evaluate) {
    const std::vector<std::tuple<std::string, int>> test_cases = {
        {board_test::fen::kings_only, 0},
        {board_test::fen::start, 0},
    };

    for (const auto& [fen, expected] : test_cases) {
        Board board(fen);
        EXPECT_EQ(eval::evaluate(board), expected + eval::tempo_bonus) << fen;
        board.make_null();
        EXPECT_EQ(eval::evaluate(board), expected + eval::tempo_bonus) << fen;
    }
}

TEST(EvaluatorTest, SideToMoveOnlyChangesPerspectiveAndTempo) {
    const Board white_to_move(board_test::fen::white_pawn_e2);
    const Board black_to_move("4k3/8/8/8/8/8/4P3/4K3 b - - 0 1");

    const int white_eval = eval::evaluate(white_to_move);
    const int black_eval = eval::evaluate(black_to_move);

    EXPECT_GT(white_eval, eval::tempo_bonus);
    EXPECT_LT(black_eval, eval::tempo_bonus);
    EXPECT_EQ(white_eval + black_eval, 2 * eval::tempo_bonus);
}

TEST(EvaluatorTest, NullMoveOnlyChangesPerspectiveAndTempo) {
    Board board(board_test::fen::white_pawn_e2);

    const int white_eval = eval::evaluate(board);
    board.make_null();
    const int black_eval = eval::evaluate(board);

    EXPECT_EQ(white_eval + black_eval, 2 * eval::tempo_bonus);
}

TEST(EvaluatorTest, EvaluationIsColorSymmetric) {
    const Board original("6k1/8/2p5/4pNp1/3nP1P1/2P5/8/6K1 w - - 0 1");
    const Board color_swapped("1k6/8/5p2/1p1pN3/1PnP4/5P2/8/1K6 b - - 0 1");

    const eval::Trace original_trace = eval::evaluate_trace(original);
    const eval::Trace swapped_trace  = eval::evaluate_trace(color_swapped);

    for (int index = 0; index < static_cast<int>(eval::Term::Count); ++index) {
        SCOPED_TRACE(index);
        const auto             term          = static_cast<eval::Term>(index);
        const eval::TermScore& original_term = original_trace.term(term);
        const eval::TermScore& swapped_term  = swapped_trace.term(term);

        EXPECT_EQ(original_term.per_color, swapped_term.per_color);
        EXPECT_EQ(original_term.total(), -swapped_term.total());
        if (original_term.per_color) {
            EXPECT_EQ(original_term.white, swapped_term.black);
            EXPECT_EQ(original_term.black, swapped_term.white);
        }
    }

    EXPECT_EQ(original_trace.unscaled_score(), -swapped_trace.unscaled_score());
    EXPECT_EQ(original_trace.scaled_score(), -swapped_trace.scaled_score());
    EXPECT_EQ(original_trace.tapered_value(), -swapped_trace.tapered_value());
    EXPECT_EQ(original_trace.side_to_move_value(), swapped_trace.side_to_move_value());
    EXPECT_EQ(original_trace.value(), swapped_trace.value());
    EXPECT_EQ(original_trace.white_value(), -swapped_trace.white_value());
    EXPECT_EQ(original_trace.side_to_move(), ~swapped_trace.side_to_move());
}

TEST(EvaluatorTest, TraceMatchesNormalEvaluationAndFormatsStableTermBreakdown) {
    const Board       board(board_test::fen::start);
    const eval::Trace trace  = eval::evaluate_trace(board);
    const std::string output = eval::format_trace(trace);

    EXPECT_EQ(trace.value(), eval::evaluate(board));
    EXPECT_EQ(trace.term_total(), trace.unscaled_score());
    EXPECT_EQ(trace.side_to_move_value() + eval::tempo_bonus, trace.value());

    EXPECT_NE(output.find("Term"), std::string::npos);
    EXPECT_NE(output.find("Material"), std::string::npos);
    EXPECT_NE(output.find("Piece Sq."), std::string::npos);
    EXPECT_NE(output.find("Mobility"), std::string::npos);
    EXPECT_NE(output.find("Evaluation:"), std::string::npos);
}
