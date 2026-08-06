#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "core/constants.hpp"
#include "core/move.hpp"

namespace search {

class TranspositionTable;
extern TranspositionTable tt;

enum class TTBound : std::uint8_t {
    None,
    Exact,
    LowerBound,
    UpperBound,
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

// Decoded compact TT payload record; search/eval arithmetic stays wider at API boundaries.
struct TTRecord {
    Move         move       = NULL_MOVE;
    std::int16_t score      = 0;
    std::uint8_t depth      = 0;
    std::uint8_t generation = 0;
    TTBound      bound      = TTBound::None;

    [[nodiscard]] bool is_valid() const {
        switch (bound) {
        case TTBound::Exact:
        case TTBound::LowerBound:
        case TTBound::UpperBound: return true;
        case TTBound::None:       return false;
        }
        return false;
    }
    int                replacement_score(int current_generation) const noexcept;
    EvalValue          score_at_ply(int ply) const noexcept;
    [[nodiscard]] bool can_cutoff(EvalValue adjusted_score,
                                  int       search_depth,
                                  EvalValue alpha,
                                  EvalValue beta) const noexcept;
};
static_assert(sizeof(TTRecord) == 8);

struct alignas(64) TTCluster {
    static constexpr int size = 4;

    TTEntry entries[size] = {};
};

class TranspositionTable {
public:
    explicit TranspositionTable();

    // Shared probes return detached, validated snapshots. Stores publish the payload before its
    // full-key XOR signature, so races produce a miss or a complete old or new record.
    [[nodiscard]] std::optional<TTRecord> probe(PositionKey zkey) const;
    void store(PositionKey zkey, Move move, EvalValue score, int depth, TTBound bound, int ply);
    void resize(size_t megabytes);
    void clear();
    [[nodiscard]] std::size_t capacity_mb() const noexcept;
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
