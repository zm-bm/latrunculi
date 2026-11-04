#include "movegen/generator.hpp"

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

MoveList generate_pseudo_legal(const Board& board) {
    return board.is_check() ? generate_list<MoveGenType::Evasions>(board)
                            : generate_list<MoveGenType::NonEvasions>(board);
}

} // namespace movegen::detail

namespace movegen {

LegalMoves::LegalMoves(const Board& board)
    : board(board),
      moves(detail::generate_pseudo_legal(board)) {}

} // namespace movegen
