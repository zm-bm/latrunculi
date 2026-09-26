#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "core/constants.hpp"
#include "core/move.hpp"

namespace search {

class TranspositionTable;
extern TranspositionTable tt;

enum class TTBound : std::uint8_t {
    None       = 0,
    Exact      = 1,
    LowerBound = 2,
    UpperBound = 3,
};

[[nodiscard]] constexpr TTBound
tt_bound_for_window(EvalValue value, EvalValue alpha, EvalValue beta) noexcept {
    if (value >= beta)
        return TTBound::LowerBound;
    if (value <= alpha)
        return TTBound::UpperBound;
    return TTBound::Exact;
}

struct TTEntry {
    std::atomic<std::uint64_t> payload   = 0;
    std::atomic<std::uint64_t> signature = 0;
};
static_assert(sizeof(TTEntry) == 16);

// Decoded compact TT payload record; search/eval arithmetic stays wider at API boundaries.
struct TTRecord {
    // Reserve the top signed 16-bit value for an unavailable static evaluation. Values outside the
    // cacheable domain are stored as unavailable and calculated again on a later hit.
    static constexpr std::int16_t no_static_eval = std::numeric_limits<std::int16_t>::max();

    Move         move        = NULL_MOVE;
    std::int16_t score       = 0;
    std::uint8_t depth       = 0;
    std::uint8_t generation  = 0;
    TTBound      bound       = TTBound::None;
    std::int16_t static_eval = no_static_eval;

    [[nodiscard]] bool is_valid() const noexcept {
        switch (bound) {
        case TTBound::Exact:
        case TTBound::LowerBound:
        case TTBound::UpperBound: return true;
        case TTBound::None:       return false;
        }
        return false;
    }
    [[nodiscard]] bool has_static_eval() const noexcept { return static_eval != no_static_eval; }
    int                replacement_score(int current_generation) const noexcept;
    EvalValue          score_at_ply(int ply) const noexcept;
    [[nodiscard]] bool can_cutoff(EvalValue adjusted_score,
                                  int       search_depth,
                                  EvalValue alpha,
                                  EvalValue beta) const noexcept;
};
static_assert(sizeof(TTRecord) == 10);

struct alignas(64) TTCluster {
    static constexpr int size = 4;

    TTEntry entries[size] = {};
};
static_assert(sizeof(TTCluster) == 64);
static_assert(alignof(TTCluster) == 64);

class TranspositionTable {
public:
    explicit TranspositionTable();

    // Shared probes return detached, validated snapshots. Stores publish the payload before its
    // full-key XOR signature, so races produce a miss or a complete old or new record.
    void                                  prefetch(PositionKey zkey) const noexcept;
    [[nodiscard]] std::optional<TTRecord> probe(PositionKey zkey) const;
    void                                  store(PositionKey zkey,
                                                Move        move,
                                                EvalValue   score,
                                                int         depth,
                                                TTBound     bound,
                                                int         ply,
                                                EvalValue   static_eval = TTRecord::no_static_eval);
    void                                  resize(size_t megabytes);
    void                                  clear();
    [[nodiscard]] std::size_t             capacity_mb() const noexcept;
    // Advance the shared TT generation once per root-search lifecycle event.
    void                       advance_generation() { ++generation; }
    [[nodiscard]] std::uint8_t current_generation() const { return generation; }

private:
    std::uint64_t cluster_index(PositionKey zkey) const;

    std::unique_ptr<TTCluster[]> clusters = nullptr;

    size_t       cluster_count = 0;
    int          shift         = 0;
    std::uint8_t generation    = 0;
};

inline std::uint64_t TranspositionTable::cluster_index(PositionKey zkey) const {
    return (zkey * 0x9e3779b97f4a7c15ull) >> shift;
}

inline void TranspositionTable::prefetch(PositionKey zkey) const noexcept {
    __builtin_prefetch(&clusters[cluster_index(zkey)]);
}

// lower score = better replacement candidate: prefer shallow entries, then older entries
inline int TTRecord::replacement_score(int current_generation) const noexcept {
    const int age_distance = std::uint8_t(std::uint8_t(current_generation) - generation);
    return int(depth) - 4 * age_distance;
}

// Convert a stored current-position mate score back into a root-relative search score.
inline EvalValue TTRecord::score_at_ply(int ply) const noexcept {
    const EvalValue value = score;

    if (value >= eval_value::tt_mate_bound)
        return value - ply;
    if (value <= -eval_value::tt_mate_bound)
        return value + ply;
    return value;
}

inline bool TTRecord::can_cutoff(EvalValue adjusted_score,
                                 int       search_depth,
                                 EvalValue alpha,
                                 EvalValue beta) const noexcept {
    assert(search_depth >= 0 && search_depth <= engine::max_search_ply);

    if (int(depth) < search_depth)
        return false;

    switch (bound) {
    case TTBound::Exact:      return true;
    case TTBound::LowerBound: return adjusted_score >= beta;
    case TTBound::UpperBound: return adjusted_score <= alpha;
    case TTBound::None:       return false;
    }

    return false;
}

} // namespace search
