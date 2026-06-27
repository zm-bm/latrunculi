#include "movegen.hpp"

#include "movegen_impl.hpp"

#include <cassert>

namespace movegen {

namespace {
template <MoveGenType M>
MoveList generate_movelist(const Board& board) {
    MoveList moves;
    generate_into<M>(board, moves);
    return moves;
}
} // namespace

MoveList generate_non_evasions(const Board& board) {
    assert(!board.is_check());
    return generate_movelist<MoveGenType::NonEvasions>(board);
}

MoveList generate_noisy(const Board& board) {
    assert(!board.is_check());
    return generate_movelist<MoveGenType::Noisy>(board);
}

MoveList generate_quiet(const Board& board) {
    assert(!board.is_check());
    return generate_movelist<MoveGenType::Quiet>(board);
}

MoveList generate_evasions(const Board& board) {
    assert(board.is_check());
    return generate_movelist<MoveGenType::Evasions>(board);
}

MoveList generate_pseudo_legal(const Board& board) {
    return board.is_check() ? generate_evasions(board) : generate_non_evasions(board);
}

} // namespace movegen
