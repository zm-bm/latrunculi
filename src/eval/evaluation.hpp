#pragma once

#include "core/types.hpp"
#include "eval/trace.hpp"

class Board;

namespace eval {

[[nodiscard]] EvalValue evaluate(const Board& board);
[[nodiscard]] Trace     evaluate_trace(const Board& board);

} // namespace eval
