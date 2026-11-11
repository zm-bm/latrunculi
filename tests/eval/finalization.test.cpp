#include "eval/score_finalization.hpp"

#include <gtest/gtest.h>

#include "eval/features.hpp"

namespace {
eval::FinalizationContext bishops() {
    eval::FinalizationContext c;
    c.pieces[WHITE] = {4, 0, 1, 0, 0};
    c.pieces[BLACK] = {3, 0, 1, 0, 0};
    c.pawn_flanks = {3, 3};
    c.bishop_colors[WHITE] = 1;
    c.bishop_colors[BLACK] = 2;
    return c;
}
} // namespace

TEST(FinalizationTest, OppositeBishopsRequireExactMatchingComposition) {
    auto c = bishops();
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 32);
    c.bishop_colors[BLACK] = 1;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 64);
    c.bishop_colors[BLACK] = 2;
    c.pieces[WHITE][KNIGHT - PAWN] = c.pieces[BLACK][KNIGHT - PAWN] = 1;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 48);
    c.pieces[BLACK][KNIGHT - PAWN] = 2;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 64);
    c.pieces[WHITE][KNIGHT - PAWN] = c.pieces[BLACK][KNIGHT - PAWN] = 0;
    c.pieces[WHITE][ROOK - PAWN] = c.pieces[BLACK][ROOK - PAWN] = 1;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 48);
    c.pieces[WHITE][QUEEN - PAWN] = c.pieces[BLACK][QUEEN - PAWN] = 1;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 64);
    c = bishops();
    c.pieces[WHITE][BISHOP - PAWN] = 2;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 64);
}

TEST(FinalizationTest, PawnlessCapDoesNotApplyToBareKingOrLargeAdvantages) {
    auto c = bishops();
    c.pieces[WHITE] = {0, 1, 1, 0, 0};
    c.pieces[BLACK] = {0, 1, 0, 0, 0};
    c.pawn_flanks = {};
    EXPECT_EQ(eval::conversion_target(c, WHITE, 48), 16); // exactly one bishop ahead
    EXPECT_EQ(eval::conversion_target(c, BLACK, 48), 48); // materially behind
    c.pieces[WHITE] = {0, 0, 2, 0, 0};
    EXPECT_EQ(eval::conversion_target(c, WHITE, 48), 48); // just above bishop threshold
    c.pieces[BLACK] = {};
    EXPECT_EQ(eval::conversion_target(c, WHITE, 48), 48);
    c.pieces[WHITE] = {0, 0, 0, 1, 0};
    EXPECT_EQ(eval::conversion_target(c, WHITE, 48), 48);
    c.pieces[WHITE] = {0, 0, 0, 0, 1};
    EXPECT_EQ(eval::conversion_target(c, WHITE, 48), 48);
    c.pieces[WHITE] = c.pieces[BLACK] = {0, 1, 0, 0, 0};
    EXPECT_EQ(eval::conversion_target(c, WHITE, 48), 16); // equal material
}

TEST(FinalizationTest, FlanksAreColorSpecificAndCapsDoNotMultiply) {
    auto c = bishops();
    c.bishop_colors[BLACK] = 1;
    c.pawn_flanks[WHITE] = 1;
    c.pawn_flanks[BLACK] = 2;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 60);
    EXPECT_EQ(eval::conversion_target(c, BLACK, 60), 56);
    c.bishop_colors[BLACK] = 2;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 64), 32);
    c.pawn_flanks[WHITE] = 0;
    c.pieces[WHITE][PAWN - PAWN] = 0;
    EXPECT_EQ(eval::conversion_target(c, WHITE, 48), 16);
}

TEST(FinalizationTest, ScalingPreservesSignAndTruncatesTowardZero) {
    const auto c = bishops();
    for (int eg : {-1, 0, 1}) {
        const auto score = eval::finalize_score({123, eg}, c, WHITE, 64);
        EXPECT_EQ(score.scaled.eg, 0);
        EXPECT_EQ(score.scaled.mg, 123);
    }
    EXPECT_EQ(eval::finalize_score({0, 101}, c, WHITE, 64).scaled.eg, 50);
    EXPECT_EQ(eval::finalize_score({0, -101}, c, WHITE, 64).scaled.eg, -50);
    const auto partial = eval::finalize_score({0, -133}, c, BLACK, 1);
    EXPECT_EQ(partial.effective_scale, 60); // reduction 28/64 truncates to zero
    EXPECT_EQ(partial.scaled.eg, -124);
    EXPECT_EQ(eval::finalize_score({0, 101}, c, WHITE, 63).effective_scale, 33);
    EXPECT_EQ(eval::finalize_score({0, 0}, c, WHITE, 64).scaled.eg, 0);
}

TEST(FinalizationTest, NonzeroWeightsRemainColorSymmetric) {
    auto c = bishops();
    c.pawn_flanks[WHITE] = 1;
    auto swapped = c;
    std::swap(swapped.pieces[WHITE], swapped.pieces[BLACK]);
    std::swap(swapped.pawn_flanks[WHITE], swapped.pawn_flanks[BLACK]);
    std::swap(swapped.bishop_colors[WHITE], swapped.bishop_colors[BLACK]);
    const int strength = 37;
    const auto a = eval::finalize_score({107, 331}, c, WHITE, strength);
    const auto b = eval::finalize_score({-107, -331}, swapped, BLACK, strength);
    EXPECT_EQ(a.scaled, -b.scaled);
    EXPECT_EQ(a.value, b.value);
}

TEST(FinalizationTest, ConversionCapsDoNotDeclareAdvancedPasserDrawn) {
    const Board passer("6k1/2P5/8/8/8/8/7p/2B1Kb2 w - - 0 1");
    const auto c = eval::FinalizationContext::from_board(passer);
    const auto score = eval::finalize_score({0, 800}, c, WHITE, 64);
    EXPECT_EQ(score.target_scale, 32);
    EXPECT_EQ(score.scaled.eg, 400);
    EXPECT_GT(score.side_to_move, 0);
    // The heuristic preserves a large advantage; it does not adjudicate this position.
}

TEST(FinalizationTest, PhaseClampsBeforeAndAfterPromotions) {
    eval::FinalizationContext c;
    c.pieces[WHITE][PAWN - PAWN] = 4;
    c.pawn_flanks[WHITE] = 3;
    EXPECT_EQ(eval::finalize_score({123, 456}, c, WHITE, 0).tapered, 456);
    c.pieces[WHITE] = {4, 2, 2, 2, 1};
    c.pieces[BLACK] = {4, 2, 2, 2, 1};
    EXPECT_EQ(eval::finalize_score({123, 456}, c, WHITE, 0).tapered, 123);
    c.pieces[WHITE][QUEEN - PAWN] = 8;
    EXPECT_EQ(eval::finalize_score({123, 456}, c, WHITE, 0).tapered, 123);
}

TEST(FinalizationTest, ContextAndNonlinearInputsRemainReconstructible) {
    const Board board("6k1/8/8/8/8/8/P6p/2B1Kb2 w - - 0 1");
    const auto record = eval::extract_features(board);
    EXPECT_EQ(record.finalization_context.pawn_flanks[WHITE], 1);
    EXPECT_EQ(record.finalization_context.pawn_flanks[BLACK], 2);
    EXPECT_EQ(record.finalization_context.bishop_colors[WHITE]
                  ^ record.finalization_context.bishop_colors[BLACK],
              3);
    EXPECT_EQ(record.coefficients[eval::feature::scale_strength], 1);
    EXPECT_EQ(record.reconstruct(), record.value);
    EXPECT_EQ(record.fixed_score, record.term(eval::Term::KingSafety).total());
}
