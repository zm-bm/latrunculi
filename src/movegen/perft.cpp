#include "movegen/perft.hpp"

#include "board/board.hpp"
#include "movegen/movegen.hpp"
#include "board/position_state.hpp"

#include <sstream>
#include <stdexcept>

namespace {

void validate_perft_depth(int depth) {
    if (depth < 0 || depth > MAX_SEARCH_PLY)
        throw std::invalid_argument("perft depth out of range");
}

uint64_t perft_impl(Board&              board,
                    int                 depth,
                    PositionStateStack& states,
                    int                 ply,
                    PositionState&      current_state) {
    if (depth == 0)
        return 1;

    uint64_t nodes    = 0;
    auto     movelist = movegen::generate_pseudo_legal(board);

    for (auto& move : movelist) {
        if (!board.is_legal_generated_move(move))
            continue;

        board.make(move, states.child(ply));
        uint64_t count = perft_impl(board, depth - 1, states, ply + 1, states.child(ply));
        nodes += count;
        board.unmake(current_state);
    }

    return nodes;
}

} // namespace

uint64_t perft(Board& board, int depth) {
    validate_perft_depth(depth);

    PositionStateStack states;
    return perft_impl(board, depth, states, 0, board.position_state());
}

PerftResult perft_root(Board& board, int depth) {
    validate_perft_depth(depth);

    PerftResult result{
        .nodes      = 0,
        .root_moves = {},
    };

    if (depth == 0) {
        result.nodes = 1;
        return result;
    }

    PositionStateStack states;
    PositionState&     root_state = board.position_state();
    auto               movelist   = movegen::generate_pseudo_legal(board);

    for (auto& move : movelist) {
        if (!board.is_legal_generated_move(move))
            continue;

        board.make(move, states.child(0));
        const uint64_t nodes = perft_impl(board, depth - 1, states, 1, states.child(0));
        result.nodes += nodes;
        result.root_moves.push_back({.move = move, .nodes = nodes});
        board.unmake(root_state);
    }

    return result;
}

std::string format_perft_result(const PerftResult& result) {
    std::ostringstream output;
    for (const auto& root_move : result.root_moves)
        output << root_move.move.str() << ": " << root_move.nodes << '\n';
    output << "NODES: " << result.nodes << '\n';
    return output.str();
}
