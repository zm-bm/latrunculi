#pragma once

#include <array>
#include <atomic>
#include <memory>
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
    U16 key16     = 0;
    Move move     = NullMove;
    int score     = 0;
    U8 depth      = 0;
    NodeType flag = NONE;
    U8 age        = 0;
};

struct Cluster {
    Entry entries[CLUSTER_SIZE] = {};
    Spinlock lk;
};

class Table {
   private:
    std::unique_ptr<Cluster[]> _table = nullptr;
    U32 _mask                         = 0;
    U32 _shift                        = 0;
    U8 _age                           = 0;

    static U64 index(U64 k, U32 shift) { return (k * 0x9e3779b97f4a7c15ull) >> shift; }

   public:
    explicit Table() { resize(DEFAULT_HASH_MB); };

    void resize(size_t megabytes) {
        U64 bytes = U64(megabytes) << 20;

        // Ensure clusters is a power of two
        U64 clusters = bytes / sizeof(Cluster);
        clusters     = 1ULL << std::bit_width(clusters - 1);

        _table = std::make_unique<Cluster[]>(clusters);
        _mask  = U32(clusters) - 1;
        _shift = 64 - std::countr_zero(clusters);
        _age   = 0;
    }

    void age() { ++_age; }

    Entry* probe(U64 key) {
        auto idx         = index(key, _shift);
        auto key16       = U16(key >> 48);
        Cluster& cluster = _table[idx];

        for (Entry& e : cluster.entries) {
            if (e.key16 == key16 && e.flag != NONE) return &e;
        }

        return nullptr;
    }

    void store(U64 key, Move move, int score, U8 depth, NodeType flag) {
        auto idx         = index(key, _shift);
        auto key16       = U16(key >> 48);
        Cluster& cluster = _table[idx];
        cluster.lk.lock();

        // replacement: same key, else shallowest or old-age
        Entry* target = &cluster.entries[0];
        for (Entry& e : cluster.entries) {
            if (e.key16 == key16) {
                target = &e;
                break;
            }
            if (e.age != _age || e.depth < target->depth) {
                target = &e;
            }
        }

        *target = Entry{key16, move, score, depth, flag, _age};

        cluster.lk.unlock();
    }
};

extern Table table;

}  // namespace TT
