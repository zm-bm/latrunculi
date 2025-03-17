#pragma once

#include <array>

#include "move.hpp"
#include "types.hpp"

namespace TT {

enum NodeType : U8 {
    NONE,
    EXACT,
    LOWERBOUND,
    UPPERBOUND,
};

struct Entry {
    U64 zobristKey = 0;
    Move bestMove  = {};
    int score      = 0;
    int depth      = 0;
    NodeType flag  = NONE;

    bool isValid(U64 key) const { return zobristKey == key; }
};

constexpr size_t TT_SIZE = 1 << 20;

class Table {
   private:
    std::array<Entry, TT_SIZE> table;

   public:
    Table() = default;

    Entry* probe(U64 key) { return &table[key % TT_SIZE]; }

    void store(U64 key, Move move, int score, int depth, NodeType flag) {
        Entry& entry = table[key % TT_SIZE];
        if (entry.depth <= depth) {  // Only replace if deeper search
            entry = {key, move, score, depth, flag};
        }
    }
};

extern Table table;

}  // namespace TT
