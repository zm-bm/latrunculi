#pragma once

#include "board.hpp"
#include "move_list.hpp"

namespace movegen {

MoveList generate_non_evasions(const Board& board);
MoveList generate_noisy(const Board& board);
MoveList generate_quiet(const Board& board);
MoveList generate_evasions(const Board& board);
MoveList generate_pseudo_legal(const Board& board);

} // namespace movegen
