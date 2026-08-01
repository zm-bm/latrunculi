#include "search/tt.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
constexpr int tt_score_bits      = 16;
constexpr int tt_move_bits       = 16;
constexpr int tt_depth_bits      = 8;
constexpr int tt_generation_bits = 8;
constexpr int tt_bound_bits      = 8;

constexpr int tt_move_shift       = 0;
constexpr int tt_score_shift      = tt_move_shift + tt_move_bits;
constexpr int tt_depth_shift      = tt_score_shift + tt_score_bits;
constexpr int tt_generation_shift = tt_depth_shift + tt_depth_bits;
constexpr int tt_bound_shift      = tt_generation_shift + tt_generation_bits;

constexpr std::uint64_t tt_move_mask       = (std::uint64_t{1} << tt_move_bits) - 1;
constexpr std::uint64_t tt_score_mask      = (std::uint64_t{1} << tt_score_bits) - 1;
constexpr std::uint64_t tt_depth_mask      = (std::uint64_t{1} << tt_depth_bits) - 1;
constexpr std::uint64_t tt_generation_mask = (std::uint64_t{1} << tt_generation_bits) - 1;
constexpr std::uint64_t tt_bound_mask      = (std::uint64_t{1} << tt_bound_bits) - 1;
constexpr std::uint64_t tt_signature_salt  = 0x9e3779b97f4a7c15ull;

struct TTSnapshot {
    PositionKey key = 0;
    TTRecord    record{};
};

[[nodiscard]] std::uint64_t pack_payload(const TTRecord& record) {
    const auto packed_score = std::bit_cast<std::uint16_t>(record.score);

    return (std::uint64_t(record.move.bits) << tt_move_shift)
         | ((std::uint64_t(packed_score) & tt_score_mask) << tt_score_shift)
         | ((std::uint64_t(record.depth) & tt_depth_mask) << tt_depth_shift)
         | ((std::uint64_t(record.generation) & tt_generation_mask) << tt_generation_shift)
         | ((std::uint64_t(std::to_underlying(record.bound)) & tt_bound_mask) << tt_bound_shift);
}

[[nodiscard]] TTRecord unpack_payload(std::uint64_t payload) {
    const auto packed_score = std::uint16_t((payload >> tt_score_shift) & tt_score_mask);

    TTRecord record{};
    record.move.bits  = MoveBits((payload >> tt_move_shift) & tt_move_mask);
    record.score      = std::bit_cast<std::int16_t>(packed_score);
    record.depth      = std::uint8_t((payload >> tt_depth_shift) & tt_depth_mask);
    record.generation = std::uint8_t((payload >> tt_generation_shift) & tt_generation_mask);
    record.bound      = TTBound((payload >> tt_bound_shift) & tt_bound_mask);
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

TTTable tt{};

TTTable::TTTable() {
    resize(default_mb);
}

std::optional<TTRecord> TTTable::probe(PositionKey zkey) const {
    const TTCluster& cluster = clusters[cluster_index(zkey)];

    for (const TTEntry& entry : cluster.entries) {
        auto snapshot = load_snapshot(entry);
        if (snapshot && snapshot->key == zkey)
            return snapshot->record;
    }

    return std::nullopt;
}

void TTTable::store(
    PositionKey zkey, Move move, EvalValue score, int depth, TTBound bound, int ply) {
    assert(depth >= 0 && depth <= engine::max_search_ply);
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

    const TTRecord record{
        .move       = move,
        .score      = std::int16_t(score),
        .depth      = std::uint8_t(depth),
        .generation = generation,
        .bound      = bound,
    };

    const std::uint64_t payload   = pack_payload(record);
    const std::uint64_t signature = make_signature(zkey, payload);

    target->payload.store(payload, std::memory_order_relaxed);
    target->signature.store(signature, std::memory_order_release);
}

void TTTable::clear() {
    for (std::size_t i = 0; i < cluster_count; ++i) {
        for (auto& entry : clusters[i].entries)
            clear_entry(entry);
    }
    generation = 0;
}

void TTTable::resize(size_t mb) {
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
