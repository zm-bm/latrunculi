#pragma once

#include <array>
#include <utility>

#include "core/types.hpp"
#include "eval/tapered_score.hpp"
#include "eval/types.hpp"

namespace eval {

struct TermScore {
    [[nodiscard]] TaperedScore total() const noexcept;

    TaperedScore white    = TaperedScore::Zero;
    TaperedScore black    = TaperedScore::Zero;
    bool         has_both = false;
};

// Structured result of one evaluation, including its term breakdown and each
// transformation from a white-relative tapered score to the returned value.
class Trace {
public:
    [[nodiscard]] const TermScore& term(Term term) const noexcept;
    [[nodiscard]] TaperedScore     term_total() const noexcept;
    [[nodiscard]] TaperedScore     unscaled_score() const noexcept;
    [[nodiscard]] TaperedScore     final_score() const noexcept;
    [[nodiscard]] EvalValue        tapered_value() const noexcept;
    [[nodiscard]] EvalValue        relative_value() const noexcept;
    [[nodiscard]] EvalValue        value() const noexcept;
    [[nodiscard]] EvalValue        white_value() const noexcept;
    [[nodiscard]] Color            side_to_move() const noexcept;

private:
    std::array<TermScore, std::to_underlying(Term::Count)> terms{};
    TaperedScore                                           raw_score    = TaperedScore::Zero;
    TaperedScore                                           scaled_score = TaperedScore::Zero;
    EvalValue                                              tapered      = 0;
    EvalValue                                              relative     = 0;
    EvalValue                                              final_value  = 0;
    Color                                                  turn         = WHITE;

    void record(Term term, Color color, TaperedScore score) noexcept;
    void complete(TaperedScore unscaled,
                  TaperedScore scaled,
                  EvalValue    tapered_value,
                  EvalValue    relative_value,
                  EvalValue    value,
                  Color        side_to_move) noexcept;

    friend class Evaluator;
};

} // namespace eval
