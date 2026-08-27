#pragma once

#include "core/types.hpp"

class Board;

namespace eval {

[[nodiscard]] EvalValue evaluate(const Board& board);

} // namespace eval
