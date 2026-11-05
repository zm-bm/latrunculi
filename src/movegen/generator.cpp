#include "movegen/generator.hpp"

#include "movegen/generator_detail.hpp"

namespace movegen::detail {

MoveSource<Mode::All>::MoveSource(const Board& board)
    : board(board),
      next(storage.data()) {
    Move* end = next;
    generate_pseudo_legal(board, [&](Move move, PieceType) {
        assert(end < storage.data() + storage.size());
        *end++ = move;
    });
    last = end;
}

} // namespace movegen::detail
