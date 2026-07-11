#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "core/constants.hpp"
#include "core/move.hpp"

class TT_Table;
extern TT_Table tt;

enum class TT_Flag : std::uint8_t {
    None,
    Exact,
    Lowerbound,
    Upperbound,
};

[[nodiscard]] constexpr TT_Flag
tt_bound_for_window(EvalValue value, EvalValue alpha, EvalValue beta) noexcept {
    if (value >= beta)
        return TT_Flag::Lowerbound;
    if (value <= alpha)
        return TT_Flag::Upperbound;
    return TT_Flag::Exact;
}

struct TT_Entry {
    std::atomic<std::uint64_t> payload   = 0;
    std::atomic<std::uint64_t> signature = 0;
};

// Decoded compact TT payload record; search/eval arithmetic stays wider at API boundaries.
struct TT_Record {
    Move         move  = NULL_MOVE;
    std::int16_t score = 0;
    std::uint8_t depth = 0;
    std::uint8_t age   = 0;
    TT_Flag      flag  = TT_Flag::None;

    [[nodiscard]] bool is_valid() const {
        switch (flag) {
        case TT_Flag::Exact:
        case TT_Flag::Lowerbound:
        case TT_Flag::Upperbound: return true;
        case TT_Flag::None:       return false;
        }
        return false;
    }
    int                replacement_score(int tt_age) const noexcept;
    EvalValue          score_at_ply(int ply) const noexcept;
    [[nodiscard]] bool can_cutoff(EvalValue adjusted_score,
                                  int       search_depth,
                                  EvalValue alpha,
                                  EvalValue beta) const noexcept;
};
static_assert(sizeof(TT_Record) == 8);

struct alignas(64) TT_Cluster {
    static constexpr int size = 4;

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
    //   degrade to a miss, an old hit, or a new hit rather than accepting a mixed entry.
    [[nodiscard]] std::optional<TT_Record> probe(PositionKey zkey) const;
    void store(PositionKey zkey, Move move, EvalValue score, int depth, TT_Flag flag, int ply);
    void
    store_search(PositionKey zkey, Move move, EvalValue score, int depth, TT_Flag flag, int ply);
    void resize(size_t megabytes);
    void clear();
    // Advance the shared TT generation once per root-search lifecycle event.
    void                       age_table() { ++age; }
    [[nodiscard]] std::uint8_t current_age() const { return age; }
    const void*                prefetch_addr(PositionKey zkey) const;

    static constexpr size_t default_mb = 4;

private:
    std::uint64_t cluster_key(PositionKey zkey) const;

    std::unique_ptr<TT_Cluster[]> table = nullptr;

    size_t       length = 0;
    int          shift  = 0;
    std::uint8_t age    = 0;
};

inline const void* TT_Table::prefetch_addr(PositionKey zkey) const {
    std::uint64_t idx = cluster_key(zkey);
    return &table[idx];
}

inline std::uint64_t TT_Table::cluster_key(PositionKey zkey) const {
    return (zkey * 0x9e3779b97f4a7c15ull) >> shift;
}

// lower score = better replacement candidate: prefer shallow entries, then older entries
inline int TT_Record::replacement_score(int tt_age) const noexcept {
    const int relative_age = std::uint8_t(std::uint8_t(tt_age) - age);
    return int(depth) - 4 * relative_age;
}

// Convert a stored current-position mate score back into a root-relative search score.
inline EvalValue TT_Record::score_at_ply(int ply) const noexcept {
    const EvalValue value = score;

    if (value >= eval_value::tt_mate_bound)
        return value - ply;
    if (value <= -eval_value::tt_mate_bound)
        return value + ply;
    return value;
}

inline bool TT_Record::can_cutoff(EvalValue adjusted_score,
                                  int       search_depth,
                                  EvalValue alpha,
                                  EvalValue beta) const noexcept {
    assert(search_depth >= 0 && search_depth <= engine::max_search_ply);

    if (int(depth) < search_depth)
        return false;

    switch (flag) {
    case TT_Flag::Exact:      return true;
    case TT_Flag::Lowerbound: return adjusted_score >= beta;
    case TT_Flag::Upperbound: return adjusted_score <= alpha;
    case TT_Flag::None:       return false;
    }

    return false;
}

inline void TT_Table::store_search(
    PositionKey zkey, Move move, EvalValue score, int depth, TT_Flag flag, int ply) {
    assert(depth >= 0 && depth <= engine::max_search_ply);
    assert(score >= std::numeric_limits<std::int16_t>::min() &&
           score <= std::numeric_limits<std::int16_t>::max());

    store(zkey, move, score, depth, flag, ply);
}
