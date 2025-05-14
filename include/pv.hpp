#pragma once

#include <array>
#include <vector>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

struct PrincipalVariation {
    using Line = std::vector<Move>;

    std::array<Line, MAX_DEPTH> lines;

    Line& operator[](const int ply) { return lines[ply]; }
    Move bestMove() const { return !lines[0].empty() ? lines[0][0] : NullMove; }

    void update(const int ply, const Move& move) {
        Line& line = lines[ply];
        Line& prev = lines[ply + 1];

        line.clear();
        line.push_back(move);
        line.insert(line.end(), prev.begin(), prev.end());
    }

    void clear() {
        for (auto& line : lines) {
            line.clear();
        }
    }
};
