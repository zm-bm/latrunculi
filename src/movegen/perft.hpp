#pragma once

#include <string>
#include <vector>

#include "core/move.hpp"
#include "core/types.hpp"

class Board;

struct PerftRootMove {
    Move      move;
    NodeCount nodes;
};

struct PerftResult {
    NodeCount                  nodes;
    std::vector<PerftRootMove> root_moves;
};

NodeCount   perft(Board& board, int depth);
PerftResult perft_root(Board& board, int depth);
std::string format_perft_result(const PerftResult& result);
