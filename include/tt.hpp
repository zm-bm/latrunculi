#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <numeric>
#include <thread>

#include "constants.hpp"
#include "move.hpp"
#include "types.hpp"

class TT_Table;
extern TT_Table tt;

enum class TT_Flag : U8 {
    None,
    Exact,
    Lowerbound,
    Upperbound,
};

struct TT_Entry {
    Move move    = NullMove;
    I16 score    = 0;
    U16 key      = 0;
    U8 depth     = 0;
    U8 age       = 0;
    TT_Flag flag = TT_Flag::None;

    int replacement_score(int tt_age) const;
    int get_score(int ply) const;
};

struct alignas(64) TT_Cluster {
    static constexpr U32 size = 4;
    TT_Entry entries[size]    = {};
};

class TT_Table {
   public:
    explicit TT_Table();

    TT_Entry* probe(U64 zkey);
    void store(U64 zkey, Move move, I16 score, U8 depth, TT_Flag flag, int ply);
    void resize(size_t megabytes);
    void clear();
    void ageTable() { ++age; }

   private:
    U64 cluster_key(U64 zkey);
    U16 entry_key(U64 zkey);

    std::unique_ptr<TT_Cluster[]> table = nullptr;

    size_t length = 0;
    U32 shift     = 0;
    U8 age        = 0;
};

inline U64 TT_Table::cluster_key(U64 zkey) { return (zkey * 0x9e3779b97f4a7c15ull) >> shift; }

inline U16 TT_Table::entry_key(U64 zkey) { return U16((zkey ^ (zkey >> 32)) & 0xFFFF); }

// lower score = better replacement candidate
// prefer shallow entries, then older entries
inline int TT_Entry::replacement_score(int tt_age) const { return depth * 2 - (tt_age - age); }

// convert mate from current position score into mate from root
inline int TT_Entry::get_score(int ply) const {
    if (score >= TT_MATE_BOUND) return score - ply;
    if (score <= -TT_MATE_BOUND) return score + ply;
    return score;
}
