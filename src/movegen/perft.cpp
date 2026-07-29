#include "movegen/perft.hpp"

#include "board/board.hpp"
#include "core/constants.hpp"
#include "movegen/movegen.hpp"

#include <sstream>
#include <stdexcept>

namespace {

void validate_perft_depth(int depth) {
    if (depth < 0 || depth > engine::max_search_ply)
        throw std::invalid_argument("perft depth out of range");
}

NodeCount perft_impl(Board& board, int depth) {
    if (depth == 0)
        return 1;

    NodeCount nodes    = 0;
    auto      movelist = movegen::generate_pseudo_legal(board);

    for (auto& move : movelist) {
        if (!board.is_legal_generated_move(move))
            continue;

        board.make(move);
        NodeCount count = perft_impl(board, depth - 1);
        nodes += count;
        board.unmake();
    }

    return nodes;
}

} // namespace

NodeCount perft(Board& board, int depth) {
    validate_perft_depth(depth);

    return perft_impl(board, depth);
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

    auto movelist = movegen::generate_pseudo_legal(board);

    for (auto& move : movelist) {
        if (!board.is_legal_generated_move(move))
            continue;

        board.make(move);
        const NodeCount nodes = perft_impl(board, depth - 1);
        result.nodes += nodes;
        result.root_moves.push_back({.move = move, .nodes = nodes});
        board.unmake();
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
