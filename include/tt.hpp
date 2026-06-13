#pragma once

#include <atomic>
#include <memory>
#include <optional>

#include "defs.hpp"
#include "move.hpp"

class TT_Table;
extern TT_Table tt;

enum class TT_Flag : uint8_t {
    None,
    Exact,
    Lowerbound,
    Upperbound,
};

struct TT_Entry {
    std::atomic<uint64_t> payload   = 0;
    std::atomic<uint64_t> signature = 0;
};

struct TT_Record {
    Move    move  = NULL_MOVE;
    int16_t score = 0;
    uint8_t depth = 0;
    uint8_t age   = 0;
    TT_Flag flag  = TT_Flag::None;

    [[nodiscard]] bool is_valid() const { return flag != TT_Flag::None; }
    int                replacement_score(int tt_age) const;
    int                get_score(int ply) const;
};

struct alignas(64) TT_Cluster {
    static constexpr uint32_t size = 4;

    TT_Entry entries[size] = {};
};

class TT_Table {
public:
    explicit TT_Table();

    // Shared-TT contract at the API boundary:
    // - The table storage is globally shared across search threads.
    // - Callers never receive a pointer/reference to shared TT_Entry storage.
    // - probe() returns an immutable by-value snapshot of one validated entry, or std::nullopt.
    // - Search code must use only the returned TT_Record; keeping aliases into shared storage is
    // forbidden.
    // - TT internals publish payload first and a full-key XOR signature last, so racing reads
    // degrade
    //   to a miss, an old hit, or a new hit rather than accepting a mixed entry for the probed key.
    [[nodiscard]] std::optional<TT_Record> probe(uint64_t zkey) const;
    void store(uint64_t zkey, Move move, int16_t score, uint8_t depth, TT_Flag flag, int ply);
    void resize(size_t megabytes);
    void clear();
    // Advance the shared TT generation once per root-search lifecycle event.
    void                  age_table() { ++age; }
    [[nodiscard]] uint8_t current_age() const { return age; }
    const void*           prefetch_addr(uint64_t zkey) const;

    static constexpr size_t default_mb = 4;

private:
    uint64_t cluster_key(uint64_t zkey) const;

    std::unique_ptr<TT_Cluster[]> table = nullptr;

    size_t   length = 0;
    uint32_t shift  = 0;
    uint8_t  age    = 0;
};

inline const void* TT_Table::prefetch_addr(uint64_t zkey) const {
    uint64_t idx = cluster_key(zkey);
    return &table[idx];
}

inline uint64_t TT_Table::cluster_key(uint64_t zkey) const {
    return (zkey * 0x9e3779b97f4a7c15ull) >> shift;
}

// lower score = better replacement candidate
// prefer shallow entries, then older entries
inline int TT_Record::replacement_score(int tt_age) const {
    const int relative_age = uint8_t(uint8_t(tt_age) - age);
    return depth * 2 - relative_age;
}

// convert mate from current position score into mate from root
inline int TT_Record::get_score(int ply) const {
    if (score >= TT_MATE_BOUND)
        return score - ply;
    if (score <= -TT_MATE_BOUND)
        return score + ply;
    return score;
}
