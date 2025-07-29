#pragma once

#include <memory>

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
    Move     move  = NULL_MOVE;
    int16_t  score = 0;
    uint16_t key   = 0;
    uint8_t  depth = 0;
    uint8_t  age   = 0;
    TT_Flag  flag  = TT_Flag::None;

    int replacement_score(int tt_age) const;
    int get_score(int ply) const;
};

struct alignas(64) TT_Cluster {
    static constexpr uint32_t size = 4;

    TT_Entry entries[size] = {};
};

class TT_Table {
public:
    explicit TT_Table();

    TT_Entry* probe(uint64_t zkey);
    void      store(uint64_t zkey, Move move, int16_t score, uint8_t depth, TT_Flag flag, int ply);
    void      resize(size_t megabytes);
    void      clear();
    void      age_table() { ++age; }

    static constexpr size_t default_mb = 4;

private:
    uint64_t cluster_key(uint64_t zkey);
    uint16_t entry_key(uint64_t zkey);

    std::unique_ptr<TT_Cluster[]> table = nullptr;

    size_t   length = 0;
    uint32_t shift  = 0;
    uint8_t  age    = 0;
};

inline uint64_t TT_Table::cluster_key(uint64_t zkey) {
    return (zkey * 0x9e3779b97f4a7c15ull) >> shift;
}

inline uint16_t TT_Table::entry_key(uint64_t zkey) {
    return uint16_t((zkey ^ (zkey >> 32)) & 0xFFFF);
}

// lower score = better replacement candidate
// prefer shallow entries, then older entries
inline int TT_Entry::replacement_score(int tt_age) const {
    return depth * 2 - (tt_age - age);
}

// convert mate from current position score into mate from root
inline int TT_Entry::get_score(int ply) const {
    if (score >= TT_MATE_BOUND)
        return score - ply;
    if (score <= -TT_MATE_BOUND)
        return score + ply;
    return score;
}
