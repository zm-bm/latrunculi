#include "eval/evaluator.hpp"

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

TEST(EvaluatorTest, TraceMatchesNormalEvaluationAndFormatsStableTermBreakdown) {
    const Board       board(board_test::fen::start);
    const eval::Trace trace  = eval::evaluate_trace(board);
    const std::string output = eval::format_trace(trace);

    EXPECT_EQ(trace.value(), eval::evaluate(board));
    EXPECT_EQ(trace.term_total(), trace.unscaled_score());
    EXPECT_EQ(trace.relative_value() + eval::tempo_bonus, trace.value());

    EXPECT_NE(output.find("Term"), std::string::npos);
    EXPECT_NE(output.find("Material"), std::string::npos);
    EXPECT_NE(output.find("Piece Sq."), std::string::npos);
    EXPECT_NE(output.find("Mobility"), std::string::npos);
    EXPECT_NE(output.find("Evaluation:"), std::string::npos);
}
