#pragma once

#include <immintrin.h>

#include <array>
#include <atomic>
#include <memory>
#include <numeric>
#include <thread>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

namespace TT {

constexpr size_t CLUSTER_SIZE = 4;

enum class EntryType : U8 {
    None,
    Exact,
    LowerBound,
    UpperBound,
};

struct Entry {
    Move move      = NullMove;
    I16 score      = 0;
    U16 key16      = 0;
    U8 depth       = 0;
    U8 age         = 0;
    EntryType flag = EntryType::None;
};

struct alignas(64) Cluster {
    Entry entries[CLUSTER_SIZE] = {};
};

class Table {
   private:
    std::unique_ptr<Cluster[]> _table = nullptr;
    U32 _mask                         = 0;
    U32 _shift                        = 0;
    U8 _age                           = 0;
    size_t _size                      = 0;

    U64 index(U64 k) { return (k * 0x9e3779b97f4a7c15ull) >> _shift; }

   public:
    explicit Table() : _size(DEFAULT_HASH_MB) { resize(DEFAULT_HASH_MB); };

    void resize(size_t megabytes) {
        U64 bytes = U64(megabytes) << 20;

        // Ensure clusters is a power of two
        U64 clusters = bytes / sizeof(Cluster);
        clusters     = 1ULL << std::bit_width(clusters - 1);

        _table = std::make_unique<Cluster[]>(clusters);
        _mask  = U32(clusters) - 1;
        _shift = 64 - std::countr_zero(clusters);
        _age   = 0;
        _size  = megabytes;
    }

    void age() { ++_age; }

    Entry* probe(U64 key) {
        auto idx         = index(key);
        auto key16       = U16(key >> 48);
        Cluster& cluster = _table[idx];

        for (Entry& e : cluster.entries) {
            if (e.key16 == key16 && e.flag != EntryType::None) return &e;
        }

        return nullptr;
    }

    void store(U64 key, Move move, I16 score, U8 depth, EntryType flag) {
        auto idx         = index(key);
        auto key16       = U16(key >> 48);
        Cluster& cluster = _table[idx];
        // cluster.lk.lock();

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

        *target = Entry{move, score, key16, depth, _age, flag};

        // cluster.lk.unlock();
    }

    size_t size() const { return _size; }
};

struct Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

    void lock() {
        int spins = 0;
        while (flag.test_and_set(std::memory_order_acquire)) {
            if (spins++ < 64)
                _mm_pause();
            else
                std::this_thread::yield();
        }
    }

    void unlock() { flag.clear(std::memory_order_release); }
};

extern Table table;

}  // namespace TT
