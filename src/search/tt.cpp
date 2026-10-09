#include "search/tt.hpp"

#include <bit>
#include <cstdlib>
#include <type_traits>

#if defined(__linux__)
#include <sys/mman.h>
#endif
#include <limits>
#include <utility>

namespace search {
namespace {
constexpr std::uint8_t bound_mask           = 3;
constexpr int          generation_shift     = 2;
constexpr int          age_penalty          = 4;
constexpr int          same_tag_depth_slack = 2;

[[nodiscard]] TTRecord load_record(const TTEntry& entry) {
    const auto metadata = entry.metadata.load(std::memory_order_relaxed);
    TTRecord   record;
    record.depth       = entry.depth.load(std::memory_order_relaxed);
    record.bound       = TTBound(metadata & bound_mask);
    record.generation  = (metadata >> generation_shift) & tt_generation_mask;
    record.move.bits   = entry.move.load(std::memory_order_relaxed);
    record.score       = entry.score.load(std::memory_order_relaxed);
    record.static_eval = entry.static_eval.load(std::memory_order_relaxed);
    return record;
}

[[nodiscard]] std::int16_t encode_static_eval(EvalValue value) {
    if (value < std::numeric_limits<std::int16_t>::min() || value >= TTRecord::no_static_eval)
        return TTRecord::no_static_eval;
    return std::int16_t(value);
}
} // namespace

TranspositionTable tt{};

TranspositionTable::TranspositionTable() {
    resize(engine::default_hash_mb);
}

std::size_t TranspositionTable::capacity_mb() const noexcept {
    constexpr std::size_t bytes_per_mb = std::size_t{1} << 20;
    return cluster_count * sizeof(TTCluster) / bytes_per_mb;
}

TTProbe TranspositionTable::probe(PositionKey tt_key) const {
    TTCluster& cluster = clusters[cluster_index(tt_key)];
    const auto tag     = std::uint16_t(tt_key);
    TTProbe    result;
    result.writer.slot = &cluster.entries[0];

    int lowest_replacement_value = std::numeric_limits<int>::max();

    for (TTEntry& entry : cluster.entries) {
        if (entry.key_tag.load(std::memory_order_relaxed) == tag) {
            const auto record = load_record(entry);
            if (record.is_valid()) {
                result.record      = record;
                result.writer.slot = &entry;
                return result;
            }
        }
        const auto metadata = entry.metadata.load(std::memory_order_relaxed);
        // Keep scanning after an empty slot: a matching tag may appear later.
        const int age =
            (unsigned(generation) - (metadata >> generation_shift)) & tt_generation_mask;
        // Lower values favor shallower or older entries; empty slots always win.
        const int replacement_value =
            (metadata & bound_mask)
                ? int(entry.depth.load(std::memory_order_relaxed)) - age_penalty * age
                : std::numeric_limits<int>::min();
        if (replacement_value < lowest_replacement_value) {
            lowest_replacement_value = replacement_value;
            result.writer.slot       = &entry;
        }
    }
    return result;
}

void TranspositionTable::store(PositionKey tt_key,
                               Move        move,
                               EvalValue   score,
                               int         depth,
                               TTBound     bound,
                               int         ply,
                               EvalValue   static_eval) {
    store(probe(tt_key).writer, tt_key, move, score, depth, bound, ply, static_eval);
}

void TranspositionTable::store(TTWriter    writer,
                               PositionKey tt_key,
                               Move        move,
                               EvalValue   score,
                               int         depth,
                               TTBound     bound,
                               int         ply,
                               EvalValue   static_eval) {
    assert(writer.slot);
    assert(depth >= 0 && depth <= engine::max_search_depth);
    if (bound != TTBound::Exact && bound != TTBound::LowerBound && bound != TTBound::UpperBound)
        return;

    if (score >= eval_value::tt_mate_bound)
        score += ply;
    else if (score <= -eval_value::tt_mate_bound)
        score -= ply;
    assert(score >= std::numeric_limits<std::int16_t>::min()
           && score <= std::numeric_limits<std::int16_t>::max());

    TTEntry&   entry = *writer.slot;
    const auto tag   = std::uint16_t(tt_key);
    // Recursive search or another worker may have replaced the selected slot.
    const bool same_tag       = entry.key_tag.load(std::memory_order_relaxed) == tag;
    const auto metadata       = entry.metadata.load(std::memory_order_relaxed);
    const bool matching_entry = same_tag && (metadata & bound_mask);

    // A new move can refresh an existing entry even when its deeper bound is retained.
    if (!matching_entry || !move.is_null())
        entry.move.store(move.bits, std::memory_order_relaxed);

    const int age = (unsigned(generation) - (metadata >> generation_shift)) & tt_generation_mask;
    if (matching_entry && age == 0 && bound != TTBound::Exact
        && depth + same_tag_depth_slack < int(entry.depth.load(std::memory_order_relaxed)))
        return;

    if (matching_entry && static_eval == TTRecord::no_static_eval)
        static_eval = entry.static_eval.load(std::memory_order_relaxed);
    entry.score.store(std::int16_t(score), std::memory_order_relaxed);
    entry.static_eval.store(encode_static_eval(static_eval), std::memory_order_relaxed);
    entry.depth.store(std::uint8_t(depth), std::memory_order_relaxed);
    entry.metadata.store(std::uint8_t((generation << generation_shift) | std::to_underlying(bound)),
                         std::memory_order_relaxed);
    entry.key_tag.store(tag, std::memory_order_relaxed);
}

void TranspositionTable::clear() {
    for (std::size_t i = 0; i < cluster_count; ++i) {
        for (auto& entry : clusters[i].entries) {
            entry.key_tag.store(0, std::memory_order_relaxed);
            entry.depth.store(0, std::memory_order_relaxed);
            entry.metadata.store(0, std::memory_order_relaxed);
            entry.move.store(0, std::memory_order_relaxed);
            entry.score.store(0, std::memory_order_relaxed);
            entry.static_eval.store(0, std::memory_order_relaxed);
        }
    }
    generation = 0;
}

void TranspositionTable::ClusterDeleter::operator()(TTCluster* memory) const noexcept {
    if (aligned_allocation) {
        // The aligned path constructs clusters in malloc storage, without an array cookie.
        static_assert(std::is_trivially_destructible_v<TTCluster>);
        std::free(memory);
    } else {
        delete[] memory;
    }
}

TranspositionTable::ClusterStorage TranspositionTable::allocate_clusters(std::size_t count) {
#if defined(__linux__) && defined(MADV_HUGEPAGE)
    constexpr std::size_t huge_page_size = std::size_t{2} << 20;
    const std::size_t     bytes          = count * sizeof(TTCluster);
    // Table sizes are powers of two. Advise before first touch so the kernel can
    // back whole aligned regions with huge pages; ordinary pages remain valid.
    if (bytes >= huge_page_size) {
        if (void* memory = std::aligned_alloc(huge_page_size, bytes)) {
            ClusterStorage result{static_cast<TTCluster*>(memory), ClusterDeleter{true}};
            (void)madvise(memory, bytes, MADV_HUGEPAGE);
            std::uninitialized_value_construct_n(result.get(), count);
            return result;
        }
    }
#endif
    // Preserve ordinary allocation when large alignment is unavailable or unnecessary.
    return ClusterStorage{new TTCluster[count](), ClusterDeleter{false}};
}

void TranspositionTable::resize(size_t mb) {
    if (mb == 0)
        mb = 1;

    const std::uint64_t bytes                   = mb << 20;
    const size_t        new_cluster_count       = std::bit_floor(bytes / sizeof(TTCluster));
    const int           new_cluster_index_shift = 64 - std::countr_zero(new_cluster_count);
    auto                new_clusters            = allocate_clusters(new_cluster_count);

    clusters            = std::move(new_clusters);
    cluster_count       = new_cluster_count;
    cluster_index_shift = new_cluster_index_shift;
    generation          = 0;
}

} // namespace search
