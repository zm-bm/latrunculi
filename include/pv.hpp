#pragma once

#include <array>
#include <vector>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

using MoveLine = std::vector<Move>;

struct PrincipalVariation {
    std::array<MoveLine, MAX_DEPTH> lines;

    MoveLine bestLine() const { return lines[0]; }
    Move bestMove(int ply = 0) const { return lines[ply].empty() ? NullMove : lines[ply][0]; }
    void clear(int ply) { lines[ply].clear(); }

    void update(const int ply, const Move& move) {
        MoveLine& line = lines[ply];
        MoveLine& prev = lines[ply + 1];

        line.clear();
        line.push_back(move);
        line.insert(line.end(), prev.begin(), prev.end());
    }

    void clear() {
        for (auto& line : lines) {
            line.clear();
        }
    }

    MoveLine& operator[](const int ply) { return lines[ply]; }

    operator std::string() const {
        std::string string;
        for (const auto& move : lines[0]) {
            string += move.str() + " ";
        }
        return string;
    }
};
