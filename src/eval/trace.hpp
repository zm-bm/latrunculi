#pragma once

#include <array>
#include <utility>

#include "core/types.hpp"
#include "eval/tapered_score.hpp"
#include "eval/types.hpp"

namespace eval {

struct TermScore {
    [[nodiscard]] TaperedScore total() const noexcept;

    TaperedScore white     = TaperedScore::Zero;
    TaperedScore black     = TaperedScore::Zero;
    bool         per_color = false;
};

// Structured result of one evaluation, including its term breakdown and each
// transformation from a white-relative tapered score to the returned value.
class Trace {
public:
    [[nodiscard]] const TermScore& term(Term term) const noexcept;
    [[nodiscard]] TaperedScore     term_total() const noexcept;

    // White-relative MG/EG term sum before and after endgame scaling.
    [[nodiscard]] TaperedScore unscaled_score() const noexcept;
    [[nodiscard]] TaperedScore scaled_score() const noexcept;

    // The blended white-relative value, then the side-to-move value before tempo.
    [[nodiscard]] EvalValue tapered_value() const noexcept;
    [[nodiscard]] EvalValue side_to_move_value() const noexcept;

    // The returned value after tempo, in side-to-move and White perspectives.
    [[nodiscard]] EvalValue value() const noexcept;
    [[nodiscard]] EvalValue white_value() const noexcept;
    [[nodiscard]] Color     side_to_move() const noexcept;

private:
    std::array<TermScore, std::to_underlying(Term::Count)> terms{};
    TaperedScore                                           unscaled    = TaperedScore::Zero;
    TaperedScore                                           scaled      = TaperedScore::Zero;
    EvalValue                                              tapered     = 0;
    EvalValue                                              side_value  = 0;
    EvalValue                                              final_value = 0;
    Color                                                  turn        = WHITE;

    void record(Term term, Color color, TaperedScore score) noexcept;
    void complete(TaperedScore unscaled,
                  TaperedScore scaled,
                  EvalValue    tapered_value,
                  EvalValue    side_to_move_value,
                  EvalValue    value,
                  Color        side_to_move) noexcept;

    friend class Evaluator;
};

} // namespace eval
