#pragma once

#include <array>
#include <vector>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

using PVLine = std::vector<Move>;

inline std::string toString(const PVLine& line) {
    std::string result;
    for (const auto& move : line) {
        result += move.str() + " ";
    }
    return result;
}

class PVTable {
   private:
    std::array<PVLine, MAX_DEPTH> lines;

   public:
    void update(const int ply, const Move& move);
    void clear(int ply);
    void clear();
    Move bestMove(int ply = 0) const;
    PVLine bestLine() const;
    PVLine& operator[](const int ply);
    operator std::string() const;
};

inline void PVTable::update(const int ply, const Move& move) {
    PVLine& line = lines[ply];
    PVLine& prev = lines[ply + 1];

    line.clear();
    line.push_back(move);
    line.insert(line.end(), prev.begin(), prev.end());
}

inline void PVTable::clear() {
    for (auto& line : lines) {
        line.clear();
    }
}

inline void PVTable::clear(int ply) { lines[ply].clear(); }

inline Move PVTable::bestMove(int ply) const {
    return lines[ply].empty() ? NullMove : lines[ply][0];
}

inline PVLine PVTable::bestLine() const { return lines[0]; }

inline PVLine& PVTable::operator[](const int ply) { return lines[ply]; }

inline PVTable::operator std::string() const {
    std::string string;
    for (const auto& move : lines[0]) {
        string += move.str() + " ";
    }
    return string;
}
