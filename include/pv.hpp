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

    std::string bestMove() const { return lines[0].empty() ? "" : lines[0][0].str(); }

    std::string bestLine() const {
        std::string line;
        for (const auto& move : lines[0]) {
            line += move.str() + " ";
        }
        return line;
    }

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
