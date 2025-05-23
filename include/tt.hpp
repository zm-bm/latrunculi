#pragma once

#include <array>
#include <atomic>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

namespace TT {

class Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

   public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Spin-wait (busy-wait)
        }
    }

    void unlock() { flag.clear(std::memory_order_release); }
};

constexpr int score(int score, int ply) {
    if (score >= MATE_IN_MAX_PLY)
        score += ply;
    else if (score <= MATE_IN_MAX_PLY)
        score -= ply;
    return score;
}

enum NodeType : U8 {
    NONE,
    EXACT,
    LOWERBOUND,
    UPPERBOUND,
};

struct Entry {
    U64 zobristKey = 0;
    Move bestMove  = NullMove;
    int score      = 0;
    int depth      = 0;
    NodeType flag  = NONE;
    Spinlock lock;

    Entry() = default;

    Entry(U64 key, Move move, int score, int depth, NodeType flag)
        : zobristKey(key), bestMove(move), score(score), depth(depth), flag(flag) {}

    Entry(const Entry& other) {
        zobristKey = other.zobristKey;
        bestMove   = other.bestMove;
        score      = other.score;
        depth      = other.depth;
        flag       = other.flag;
        // Note: Spinlock not copied
    }

    Entry& operator=(const Entry& other) {
        if (this == &other) return *this;
        zobristKey = other.zobristKey;
        bestMove   = other.bestMove;
        score      = other.score;
        depth      = other.depth;
        flag       = other.flag;
        // Note: Spinlock not copied
        return *this;
    }

    bool isValid(U64 key) const { return zobristKey == key; }
};

constexpr size_t TT_SIZE = 1 << 20;

class Table {
   private:
    std::array<Entry, TT_SIZE> table;

   public:
    Table() = default;

    Entry probe(U64 key) {
        Entry* entry = &table[key % TT_SIZE];
        entry->lock.lock();
        Entry copy = *entry;
        entry->lock.unlock();
        return copy;
    }

    void store(U64 key, Move move, int score, int depth, NodeType flag) {
        Entry* entry = &table[key % TT_SIZE];
        entry->lock.lock();
        if (entry->depth <= depth) {
            *entry = {key, move, score, depth, flag};
        }
        entry->lock.unlock();
    }
};

extern Table table;

}  // namespace TT
