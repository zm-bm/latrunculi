#include "eval/evaluation.hpp"

#include "board/board.hpp"
#include "eval/nnue.hpp"

namespace eval {

Score evaluate(const Board& board) {
    return nnue::evaluate(board.neural_state(), board.side_to_move());
}

} // namespace eval
