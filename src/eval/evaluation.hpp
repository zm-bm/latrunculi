#pragma once

#include "core/types.hpp"

class Board;

namespace eval {

[[nodiscard]] Score evaluate(const Board& board);

} // namespace eval
