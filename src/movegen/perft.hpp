#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/move.hpp"

class Board;

struct PerftRootMove {
    Move     move;
    uint64_t nodes;
};

struct PerftResult {
    uint64_t                   nodes;
    std::vector<PerftRootMove> root_moves;
};

uint64_t    perft(Board& board, int depth);
PerftResult perft_root(Board& board, int depth);
std::string format_perft_result(const PerftResult& result);
