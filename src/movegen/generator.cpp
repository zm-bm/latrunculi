#include "movegen/generator.hpp"

#include <cassert>

#include "movegen/generator_detail.hpp"

namespace movegen::detail {

namespace {

// One concrete sink shares ordinary geometry across generation modes.
struct MoveListOutput {
    MoveList& moves;
    void      operator()(Move move, PieceType) const { moves.add(move); }
};

template <MoveGenType Kind>
MoveList generate_list(const Board& board) {
    MoveList       moves;
    MoveListOutput output{moves};
    generate<Kind>(board, output);
    return moves;
}

} // namespace

MoveList generate_non_evasions(const Board& board) {
    assert(!board.is_check());
    return generate_list<MoveGenType::NonEvasions>(board);
}

MoveList generate_noisy(const Board& board) {
    assert(!board.is_check());
    return generate_list<MoveGenType::Noisy>(board);
}

MoveList generate_quiet(const Board& board) {
    assert(!board.is_check());
    return generate_list<MoveGenType::Quiet>(board);
}

MoveList generate_evasions(const Board& board) {
    assert(board.is_check());
    return generate_list<MoveGenType::Evasions>(board);
}

MoveList generate_pseudo_legal(const Board& board) {
    return board.is_check() ? generate_evasions(board) : generate_non_evasions(board);
}

} // namespace movegen::detail

namespace movegen {

LegalMoves::LegalMoves(const Board& board)
    : board(board),
      moves(detail::generate_pseudo_legal(board)) {}

} // namespace movegen
