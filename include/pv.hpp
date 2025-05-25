#pragma once

#include <array>
#include <vector>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

using MoveLine = std::vector<Move>;

class PrincipalVariation {
   private:
    std::array<MoveLine, MAX_DEPTH> lines;

   public:
    void update(const int ply, const Move& move);
    void clear(int ply);
    void clear();
    Move bestMove(int ply = 0) const;
    MoveLine bestLine() const;
    MoveLine& operator[](const int ply);
    operator std::string() const;
};

inline void PrincipalVariation::update(const int ply, const Move& move) {
    MoveLine& line = lines[ply];
    MoveLine& prev = lines[ply + 1];

    line.clear();
    line.push_back(move);
    line.insert(line.end(), prev.begin(), prev.end());
}

inline void PrincipalVariation::clear() {
    for (auto& line : lines) {
        line.clear();
    }
}

inline void PrincipalVariation::clear(int ply) { lines[ply].clear(); }

inline Move PrincipalVariation::bestMove(int ply) const {
    return lines[ply].empty() ? NullMove : lines[ply][0];
}

inline MoveLine PrincipalVariation::bestLine() const { return lines[0]; }

inline MoveLine& PrincipalVariation::operator[](const int ply) { return lines[ply]; }

inline PrincipalVariation::operator std::string() const {
    std::string string;
    for (const auto& move : lines[0]) {
        string += move.str() + " ";
    }
    return string;
}
