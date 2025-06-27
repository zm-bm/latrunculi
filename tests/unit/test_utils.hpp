#pragma once

#include <numeric>
#include <vector>

#include "bb.hpp"
#include "types.hpp"

static U64 targets(const std::vector<Square>& squares) {
    auto addSquare = [](U64 acc, Square sq) { return acc | BB::set(sq); };
    return std::accumulate(squares.begin(), squares.end(), 0ULL, addSquare);
}
