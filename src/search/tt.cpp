#include "search/tt.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <utility>

namespace search {

namespace {
constexpr int tt_move_bits        = 16;
constexpr int tt_score_bits       = 16;
constexpr int tt_static_eval_bits = 16;
constexpr int tt_metadata_bits    = 16;

constexpr int tt_move_shift        = 0;
constexpr int tt_score_shift       = tt_move_shift + tt_move_bits;
constexpr int tt_static_eval_shift = tt_score_shift + tt_score_bits;
constexpr int tt_metadata_shift    = tt_static_eval_shift + tt_static_eval_bits;

constexpr std::uint64_t tt_move_mask        = (std::uint64_t{1} << tt_move_bits) - 1;
constexpr std::uint64_t tt_score_mask       = (std::uint64_t{1} << tt_score_bits) - 1;
constexpr std::uint64_t tt_static_eval_mask = (std::uint64_t{1} << tt_static_eval_bits) - 1;
constexpr std::uint64_t tt_metadata_mask    = (std::uint64_t{1} << tt_metadata_bits) - 1;
constexpr std::uint64_t tt_signature_salt   = 0x9e3779b97f4a7c15ull;

constexpr std::uint32_t tt_generation_count =
    std::uint32_t(std::numeric_limits<std::uint8_t>::max()) + 1;
constexpr std::uint32_t tt_valid_bound_count = 3;
constexpr std::uint32_t tt_metadata_state_count =
    std::uint32_t(engine::max_search_depth + 1) * tt_generation_count * tt_valid_bound_count;

static_assert(tt_metadata_shift + tt_metadata_bits == std::numeric_limits<std::uint64_t>::digits);
static_assert(tt_metadata_state_count <= tt_metadata_mask);

// Metadata code zero denotes an invalid record; valid states occupy 1..tt_metadata_state_count.
constexpr std::uint16_t tt_metadata_max = std::uint16_t(tt_metadata_state_count);

struct TTSnapshot {
    PositionKey key = 0;
    TTRecord    record{};
};

[[nodiscard]] std::uint16_t pack_metadata(const TTRecord& record) {
    if (!record.is_valid() || record.depth > engine::max_search_depth)
        return 0;

    const auto bound_index = std::uint32_t(std::to_underlying(record.bound) - 1);
    const auto depth_generation =
        std::uint32_t(record.depth) * tt_generation_count + record.generation;
    return std::uint16_t(1 + depth_generation * tt_valid_bound_count + bound_index);
}

void unpack_metadata(std::uint16_t metadata, TTRecord& record) {
    if (metadata == 0 || metadata > tt_metadata_max)
        return;

    std::uint32_t value = metadata - 1;
    record.bound        = TTBound(1 + value % tt_valid_bound_count);
    value /= tt_valid_bound_count;
    record.generation = std::uint8_t(value % tt_generation_count);
    record.depth      = std::uint8_t(value / tt_generation_count);
}

[[nodiscard]] std::int16_t encode_static_eval(EvalValue value) {
    if (value < std::numeric_limits<std::int16_t>::min() || value >= TTRecord::no_static_eval)
        return TTRecord::no_static_eval;
    return std::int16_t(value);
}

[[nodiscard]] std::uint64_t pack_payload(const TTRecord& record) {
    const auto packed_score       = std::bit_cast<std::uint16_t>(record.score);
    const auto packed_static_eval = std::bit_cast<std::uint16_t>(record.static_eval);
    const auto packed_metadata    = pack_metadata(record);

    return (std::uint64_t(record.move.bits) << tt_move_shift)
         | ((std::uint64_t(packed_score) & tt_score_mask) << tt_score_shift)
         | ((std::uint64_t(packed_static_eval) & tt_static_eval_mask) << tt_static_eval_shift)
         | ((std::uint64_t(packed_metadata) & tt_metadata_mask) << tt_metadata_shift);
}

[[nodiscard]] TTRecord unpack_payload(std::uint64_t payload) {
    const auto packed_score = std::uint16_t((payload >> tt_score_shift) & tt_score_mask);
    const auto packed_static_eval =
        std::uint16_t((payload >> tt_static_eval_shift) & tt_static_eval_mask);
    const auto packed_metadata = std::uint16_t((payload >> tt_metadata_shift) & tt_metadata_mask);

    TTRecord record{};
    record.move.bits   = MoveBits((payload >> tt_move_shift) & tt_move_mask);
    record.score       = std::bit_cast<std::int16_t>(packed_score);
    record.static_eval = std::bit_cast<std::int16_t>(packed_static_eval);
    unpack_metadata(packed_metadata, record);
    return record;
}

[[nodiscard]] std::uint64_t make_signature(PositionKey zkey, std::uint64_t payload) {
    return zkey ^ payload ^ tt_signature_salt;
}

[[nodiscard]] PositionKey recover_key(std::uint64_t signature, std::uint64_t payload) {
    return signature ^ payload ^ tt_signature_salt;
}

[[nodiscard]] std::optional<TTSnapshot> load_snapshot(const TTEntry& entry) {
    const std::uint64_t signature_before = entry.signature.load(std::memory_order_acquire);
    const std::uint64_t payload          = entry.payload.load(std::memory_order_relaxed);
    const std::uint64_t signature_after  = entry.signature.load(std::memory_order_acquire);
    if (signature_before != signature_after)
        return std::nullopt;

    TTRecord record = unpack_payload(payload);
    if (!record.is_valid())
        return std::nullopt;

    return TTSnapshot{.key = recover_key(signature_after, payload), .record = record};
}

void clear_entry(TTEntry& entry) {
    entry.payload.store(0, std::memory_order_relaxed);
    entry.signature.store(0, std::memory_order_relaxed);
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

std::optional<TTRecord> TranspositionTable::probe(PositionKey zkey) const {
    const TTCluster& cluster = clusters[cluster_index(zkey)];

    for (const TTEntry& entry : cluster.entries) {
        auto snapshot = load_snapshot(entry);
        if (snapshot && snapshot->key == zkey)
            return snapshot->record;
    }

    return std::nullopt;
}

void TranspositionTable::store(PositionKey zkey,
                               Move        move,
                               EvalValue   score,
                               int         depth,
                               TTBound     bound,
                               int         ply,
                               EvalValue   static_eval) {
    assert(depth >= 0 && depth <= engine::max_search_depth);
    assert(score >= std::numeric_limits<std::int16_t>::min()
           && score <= std::numeric_limits<std::int16_t>::max());

    TTCluster& cluster = clusters[cluster_index(zkey)];

    // convert mate from root score into mate from current position
    if (score >= eval_value::tt_mate_bound)
        score += ply;
    else if (score <= -eval_value::tt_mate_bound)
        score -= ply;
    assert(score >= std::numeric_limits<std::int16_t>::min()
           && score <= std::numeric_limits<std::int16_t>::max());

    // replacement policy: prefer same key, then lowest replacement score
    TTEntry* target = &cluster.entries[0];
    TTRecord target_record{};
    int      target_replacement_score = std::numeric_limits<int>::max();
    bool     target_is_same_key       = false;

    for (TTEntry& entry : cluster.entries) {
        auto snapshot = load_snapshot(entry);
        if (snapshot && snapshot->key == zkey) {
            if (bound != TTBound::Exact && depth + 2 < int(snapshot->record.depth))
                return;

            target             = &entry;
            target_record      = snapshot->record;
            target_is_same_key = true;
            break;
        }

        const int replacement_score = snapshot ? snapshot->record.replacement_score(generation)
                                               : std::numeric_limits<int>::min();
        if (!target_is_same_key && replacement_score < target_replacement_score) {
            target                   = &entry;
            target_replacement_score = replacement_score;
        }
    }

    if (target_is_same_key && move.is_null())
        move = target_record.move;
    if (target_is_same_key && static_eval == TTRecord::no_static_eval
        && target_record.has_static_eval())
        static_eval = target_record.static_eval;

    const TTRecord record{
        .move        = move,
        .score       = std::int16_t(score),
        .depth       = std::uint8_t(depth),
        .generation  = generation,
        .bound       = bound,
        .static_eval = encode_static_eval(static_eval),
    };

    const std::uint64_t payload   = pack_payload(record);
    const std::uint64_t signature = make_signature(zkey, payload);

    target->payload.store(payload, std::memory_order_relaxed);
    target->signature.store(signature, std::memory_order_release);
}

void TranspositionTable::clear() {
    for (std::size_t i = 0; i < cluster_count; ++i) {
        for (auto& entry : clusters[i].entries)
            clear_entry(entry);
    }
    generation = 0;
}

void TranspositionTable::resize(size_t mb) {
    if (mb == 0)
        mb = 1;

    const std::uint64_t bytes             = mb << 20;
    const size_t        new_cluster_count = std::bit_floor(bytes / sizeof(TTCluster));
    const int           new_shift         = 64 - std::countr_zero(new_cluster_count);
    auto                new_clusters      = std::make_unique<TTCluster[]>(new_cluster_count);

    clusters      = std::move(new_clusters);
    cluster_count = new_cluster_count;
    shift         = new_shift;
    generation    = 0;
}

} // namespace search
