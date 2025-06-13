#pragma once

#include <array>
#include <atomic>
#include <numeric>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

namespace TT {

constexpr size_t CLUSTER_SIZE = 4;

inline constexpr int score(int score, int ply) {
    if (score >= MATE_IN_MAX_PLY)
        score += ply;
    else if (score <= MATE_IN_MAX_PLY)
        score -= ply;
    return score;
}

struct Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Spin-wait (busy-wait)
        }
    }

    void unlock() { flag.clear(std::memory_order_release); }
};

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
};

struct Cluster {
    Entry entries[CLUSTER_SIZE] = {};
    Spinlock lk;
};

class Table {
   private:
    Cluster* _table = nullptr;
    U32 _mask       = 0;

   public:
    Table() { init(DEFAULT_HASH_MB); };
    ~Table() { delete[] _table; }

    void init(size_t megabytes) {
        delete[] _table;

        U64 bytes    = U64(megabytes) << 20;
        U64 clusters = bytes / sizeof(Cluster);
        clusters     = 1ULL << std::bit_width(clusters - 1);

        _mask  = U32(clusters) - 1;
        _table = new Cluster[clusters]();
    }

    Entry* probe(U64 key) {
        Cluster& cluster = _table[key & _mask];

        for (Entry& e : cluster.entries) {
            if (e.zobristKey == key && e.flag != NONE) return &e;
        }

        return nullptr;
    }

    void store(U64 key, Move move, int score, int depth, NodeType flag) {
        Cluster& cluster = _table[key & _mask];
        cluster.lk.lock();
        Entry* replace = &cluster.entries[0];

        // replacement strategy: deepest-first else overwrite the shallowest
        for (Entry& e : cluster.entries) {
            if (e.zobristKey == key) {
                replace = &e;
                break;
            }
            if (e.depth < replace->depth) {
                replace = &e;
            }
        }
        *replace = Entry{key, move, score, depth, flag};

        cluster.lk.unlock();
    }
};

extern Table table;

}  // namespace TT
