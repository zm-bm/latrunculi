#pragma once

#include <numeric>
#include <vector>

#include "bb.hpp"
#include "types.hpp"

const auto E2E4 = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";

static U64 targets(const std::vector<Square>& squares) {
    auto addSquare = [](U64 acc, Square sq) { return acc | BB::set(sq); };
    return std::accumulate(squares.begin(), squares.end(), 0ULL, addSquare);
}
