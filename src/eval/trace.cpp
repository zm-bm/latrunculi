#include "eval/trace.hpp"

namespace eval {

TaperedScore TermScore::total() const noexcept {
    return per_color ? white - black : white;
}

const TermScore& Trace::term(Term term) const noexcept {
    return terms[std::to_underlying(term)];
}

TaperedScore Trace::term_total() const noexcept {
    TaperedScore total;
    for (const TermScore& score : terms)
        total += score.total();
    return total;
}

TaperedScore Trace::unscaled_score() const noexcept {
    return unscaled;
}

TaperedScore Trace::scaled_score() const noexcept {
    return scaled;
}

EvalValue Trace::tapered_value() const noexcept {
    return tapered;
}

EvalValue Trace::side_to_move_value() const noexcept {
    return side_value;
}

EvalValue Trace::value() const noexcept {
    return final_value;
}

EvalValue Trace::white_value() const noexcept {
    return turn == WHITE ? final_value : -final_value;
}

Color Trace::side_to_move() const noexcept {
    return turn;
}

void Trace::record(Term term, Color color, TaperedScore score) noexcept {
    TermScore& term_score = terms[std::to_underlying(term)];
    if (color == WHITE)
        term_score.white = score;
    else {
        term_score.black     = score;
        term_score.per_color = true;
    }
}

void Trace::complete(TaperedScore unscaled,
                     TaperedScore scaled,
                     EvalValue    tapered_value,
                     EvalValue    side_to_move_value,
                     EvalValue    value,
                     Color        side_to_move) noexcept {
    this->unscaled = unscaled;
    this->scaled   = scaled;
    tapered        = tapered_value;
    side_value     = side_to_move_value;
    final_value    = value;
    turn           = side_to_move;
}

} // namespace eval
