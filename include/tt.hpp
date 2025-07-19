#pragma once

// #include <immintrin.h>

#include <array>
#include <atomic>
#include <memory>
#include <numeric>
#include <thread>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

class TranspositionTable;

extern TranspositionTable tt;

enum class TT_Flag : U8 {
    None,
    Exact,
    LowerBound,
    UpperBound,
};

struct TT_Entry {
    Move move    = NullMove;
    I16 score    = 0;
    U16 key16    = 0;
    U8 depth     = 0;
    U8 age       = 0;
    TT_Flag flag = TT_Flag::None;
};

constexpr U32 TT_CLUSTER_SIZE = 4;

struct alignas(64) TT_Cluster {
    TT_Entry entries[TT_CLUSTER_SIZE] = {};
};

class TranspositionTable {
   private:
    std::unique_ptr<TT_Cluster[]> table = nullptr;

    U32 size  = 0;
    U32 mask  = 0;
    U32 shift = 0;
    U8 age    = 0;

    U64 index(U64 k) { return (k * 0x9e3779b97f4a7c15ull) >> shift; }

   public:
    explicit TranspositionTable() : size(DEFAULT_HASH_MB) { resize(DEFAULT_HASH_MB); };

    TT_Entry* probe(U64 key);

    void store(U64 key, Move move, I16 score, U8 depth, TT_Flag flag);

    void clear();
    void resize(U32 megabytes);

    void ageTable() { ++age; }
    U32 getSize() const { return size; }
};

inline TT_Entry* TranspositionTable::probe(U64 key) {
    auto idx            = index(key);
    auto key16          = U16(key >> 48);
    TT_Cluster& cluster = table[idx];

    for (TT_Entry& e : cluster.entries) {
        if (e.key16 == key16 && e.flag != TT_Flag::None) return &e;
    }

    return nullptr;
}

inline void TranspositionTable::store(U64 key, Move move, I16 score, U8 depth, TT_Flag flag) {
    auto idx            = index(key);
    auto key16          = U16(key >> 48);
    TT_Cluster& cluster = table[idx];
    // cluster.lk.lock();

    // replacement: same key, else shallowest or old-age
    TT_Entry* target = &cluster.entries[0];
    for (TT_Entry& e : cluster.entries) {
        if (e.key16 == key16) {
            target = &e;
            break;
        }
        if (e.age != age || e.depth < target->depth) {
            target = &e;
        }
    }

    *target = TT_Entry{move, score, key16, depth, age, flag};

    // cluster.lk.unlock();
}

inline void TranspositionTable::clear() {
    for (U32 i = 0; i < (mask + 1); ++i) {
        for (auto& entry : table[i].entries) {
            entry = TT_Entry{};
        }
    }
    age = 0;
}

inline void TranspositionTable::resize(U32 megabytes) {
    U64 bytes = U64(megabytes) << 20;

    // Ensure clusters is a power of two
    U64 clusters = bytes / sizeof(TT_Cluster);
    clusters     = 1ULL << std::bit_width(clusters - 1);

    table = std::make_unique<TT_Cluster[]>(clusters);
    mask  = U32(clusters) - 1;
    shift = 64 - std::countr_zero(clusters);
    age   = 0;
    size  = megabytes;
}

// struct Spinlock{
//     std::atomic_flag flag = ATOMIC_FLAG_INIT;

//     void lock() {
//         int spins = 0;
//         while (flag.test_and_set(std::memory_order_acquire)) {
//             if (spins++ < 64)
//                 _mm_pause();
//             else
//                 std::this_thread::yield();
//         }
//     }

//     void unlock() { flag.clear(std::memory_order_release); }
// };
