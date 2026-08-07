#include "eval/evaluator.hpp"

#include <algorithm>

#include "board/board.hpp"
#include "eval/parameters.hpp"

namespace eval {

Evaluator::Evaluator(const Board& board) : board{board} {
    initialize<WHITE>();
    initialize<BLACK>();
}

template <bool Tracing>
EvalValue Evaluator::evaluate_impl(Trace* trace) {
    TaperedScore score;

    // Basic terms.
    score += evaluate_term<Tracing, Term::Material>(trace);
    score += evaluate_term<Tracing, Term::Squares>(trace);
    score += evaluate_term<Tracing, Term::Pawns, WHITE>(trace)
           - evaluate_term<Tracing, Term::Pawns, BLACK>(trace);
    score += evaluate_term<Tracing, Term::Knights, WHITE>(trace)
           - evaluate_term<Tracing, Term::Knights, BLACK>(trace);
    score += evaluate_term<Tracing, Term::Bishops, WHITE>(trace)
           - evaluate_term<Tracing, Term::Bishops, BLACK>(trace);
    score += evaluate_term<Tracing, Term::Rooks, WHITE>(trace)
           - evaluate_term<Tracing, Term::Rooks, BLACK>(trace);
    score += evaluate_term<Tracing, Term::Queens, WHITE>(trace)
           - evaluate_term<Tracing, Term::Queens, BLACK>(trace);

    // Terms requiring data accumulated by basic terms.
    score += evaluate_term<Tracing, Term::King, WHITE>(trace)
           - evaluate_term<Tracing, Term::King, BLACK>(trace);
    score += evaluate_term<Tracing, Term::Mobility, WHITE>(trace)
           - evaluate_term<Tracing, Term::Mobility, BLACK>(trace);
    score += evaluate_term<Tracing, Term::Threats, WHITE>(trace)
           - evaluate_term<Tracing, Term::Threats, BLACK>(trace);

    const Color        side_to_move   = board.side_to_move();
    const Color        stronger_side  = score.eg < 0 ? BLACK : WHITE;
    const TaperedScore unscaled_score = score;
    score.eg = (score.eg * scale_factor(stronger_side)) / eval::scale_limit;

    const EvalValue tapered_value      = taper_score(score);
    const EvalValue side_to_move_value = tapered_value * (side_to_move == WHITE ? 1 : -1);
    const EvalValue final_value        = side_to_move_value + eval::tempo_bonus;

    if constexpr (Tracing) {
        trace->complete(
            unscaled_score, score, tapered_value, side_to_move_value, final_value, side_to_move);
    }

    return final_value;
}

EvalValue Evaluator::evaluate() {
    return evaluate_impl<false>(nullptr);
}

Trace Evaluator::trace() {
    Trace result;
    evaluate_impl<true>(&result);
    return result;
}

// Integer numerator for scaling endgame evaluation toward zero in drawish pawn endings.
int Evaluator::scale_factor(Color color) const {
    const int pawn_count = board.count(color, PAWN);
    return std::min(eval::scale_limit, 36 + 5 * pawn_count);
}

// Blend middlegame and endgame scores based on game phase.
EvalValue Evaluator::taper_score(TaperedScore score) const {
    const int mg_phase = phase();
    const int eg_phase = eval::phase_limit - mg_phase;

    return ((score.mg * mg_phase) + (score.eg * eg_phase)) / eval::phase_limit;
}

// Game phase: zero is endgame and phase_limit is middlegame.
int Evaluator::phase() const {
    const int non_pawn_material = board.non_pawn_material(WHITE) + board.non_pawn_material(BLACK);
    const int material = std::clamp(non_pawn_material, eval::material_eg, eval::material_mg);

    return ((material - eval::material_eg) * eval::phase_limit)
         / (eval::material_mg - eval::material_eg);
}

EvalValue evaluate(const Board& board) {
    return Evaluator(board).evaluate();
}

Trace evaluate_trace(const Board& board) {
    return Evaluator(board).trace();
}

} // namespace eval
