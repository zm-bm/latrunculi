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

// Each field is atomic; readers may observe fields from different concurrent stores.
// Metadata uses bits 0-1 for the bound, 2-6 for generation, and leaves bit 7 reserved.
// TTBound::None marks an empty slot; zero tags and zero depths are valid data.
struct TTEntry {
    std::atomic<std::uint16_t> key_tag     = 0;
    std::atomic<std::uint8_t>  depth       = 0;
    std::atomic<std::uint8_t>  metadata    = 0;
    std::atomic<MoveBits>      move        = 0;
    std::atomic<std::int16_t>  score       = 0;
    std::atomic<std::int16_t>  static_eval = 0;
};
static_assert(sizeof(TTEntry) == 10);
static_assert(std::atomic<std::uint16_t>::is_always_lock_free);
static_assert(std::atomic<std::uint8_t>::is_always_lock_free);

inline constexpr std::uint8_t tt_generation_mask = 31;

// Detached copy of the fields observed by a probe.
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
    EvalValue          score_at_ply(int ply) const noexcept;
    [[nodiscard]] bool can_cutoff(EvalValue adjusted_score,
                                  int       search_depth,
                                  EvalValue alpha,
                                  EvalValue beta) const noexcept;
};
static_assert(sizeof(TTRecord) == 10);

struct alignas(32) TTCluster {
    static constexpr int size = 3;

    TTEntry      entries[size] = {};
    std::uint8_t padding[2]    = {};
};
static_assert(sizeof(TTCluster) == 32);
static_assert(alignof(TTCluster) == 32);

// A slot selected by probe, not a reservation. Use it only with the same table and TT key.
// A default writer has no slot; resize or destruction invalidates the table's writers.
class TTWriter {
    friend class TranspositionTable;
    TTEntry* slot = nullptr;
};

// A miss has no record but still supplies a writer for a subsequent store.
struct TTProbe {
    std::optional<TTRecord> record;
    TTWriter                writer;
};

// Concurrent probes/stores use short tags: a hit does not establish full-key identity.
// Resize, clear, and generation changes require exclusive access to the table.
class TranspositionTable {
public:
    explicit TranspositionTable();

    void                  prefetch(PositionKey tt_key) const noexcept;
    [[nodiscard]] TTProbe probe(PositionKey tt_key) const;

    // Rechecks the selected slot after recursive/concurrent stores. Scores are root-relative;
    // mate scores are normalized using ply before storage.
    void store(TTWriter    writer,
               PositionKey tt_key,
               Move        move,
               EvalValue   score,
               int         depth,
               TTBound     bound,
               int         ply,
               EvalValue   static_eval = TTRecord::no_static_eval);

    // Select a writer immediately when the caller has no retained probe.
    void store(PositionKey tt_key,
               Move        move,
               EvalValue   score,
               int         depth,
               TTBound     bound,
               int         ply,
               EvalValue   static_eval = TTRecord::no_static_eval);

    void resize(size_t megabytes);
    void clear();

    [[nodiscard]] std::size_t capacity_mb() const noexcept;
    [[nodiscard]] std::size_t entry_count() const noexcept {
        return cluster_count * TTCluster::size;
    }

    // Advance once before the root search releases helpers to access the table.
    void advance_generation() { generation = (generation + 1) & tt_generation_mask; }
    [[nodiscard]] std::uint8_t current_generation() const { return generation; }

private:
    std::uint64_t cluster_index(PositionKey tt_key) const;

    struct ClusterDeleter {
        bool aligned_allocation = false;
        void operator()(TTCluster* memory) const noexcept;
    };
    using ClusterStorage = std::unique_ptr<TTCluster[], ClusterDeleter>;
    static ClusterStorage allocate_clusters(std::size_t count);

    ClusterStorage clusters{nullptr, ClusterDeleter{}};

    size_t       cluster_count       = 0;
    int          cluster_index_shift = 0;
    std::uint8_t generation          = 0;
};

inline std::uint64_t TranspositionTable::cluster_index(PositionKey tt_key) const {
    // The low bits form the tag; the high bits select the power-of-two cluster array.
    return tt_key >> cluster_index_shift;
}

inline void TranspositionTable::prefetch(PositionKey tt_key) const noexcept {
    __builtin_prefetch(&clusters[cluster_index(tt_key)]);
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
