#include "search/tt.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
constexpr int tt_score_bits = 16;
constexpr int tt_move_bits  = 16;
constexpr int tt_depth_bits = 8;
constexpr int tt_age_bits   = 8;
constexpr int tt_flag_bits  = 8;

constexpr int tt_move_shift  = 0;
constexpr int tt_score_shift = tt_move_shift + tt_move_bits;
constexpr int tt_depth_shift = tt_score_shift + tt_score_bits;
constexpr int tt_age_shift   = tt_depth_shift + tt_depth_bits;
constexpr int tt_flag_shift  = tt_age_shift + tt_age_bits;

constexpr std::uint64_t tt_move_mask      = (std::uint64_t{1} << tt_move_bits) - 1;
constexpr std::uint64_t tt_score_mask     = (std::uint64_t{1} << tt_score_bits) - 1;
constexpr std::uint64_t tt_depth_mask     = (std::uint64_t{1} << tt_depth_bits) - 1;
constexpr std::uint64_t tt_age_mask       = (std::uint64_t{1} << tt_age_bits) - 1;
constexpr std::uint64_t tt_flag_mask      = (std::uint64_t{1} << tt_flag_bits) - 1;
constexpr std::uint64_t tt_signature_salt = 0x9e3779b97f4a7c15ull;

struct TT_Snapshot {
    PositionKey key = 0;
    TT_Record   record{};
};

[[nodiscard]] std::uint64_t pack_payload(const TT_Record& record) {
    const auto packed_score = std::bit_cast<std::uint16_t>(record.score);

    return (std::uint64_t(record.move.bits) << tt_move_shift) |
           ((std::uint64_t(packed_score) & tt_score_mask) << tt_score_shift) |
           ((std::uint64_t(record.depth) & tt_depth_mask) << tt_depth_shift) |
           ((std::uint64_t(record.age) & tt_age_mask) << tt_age_shift) |
           ((std::uint64_t(std::to_underlying(record.flag)) & tt_flag_mask) << tt_flag_shift);
}

[[nodiscard]] TT_Record unpack_payload(std::uint64_t payload) {
    const auto packed_score = std::uint16_t((payload >> tt_score_shift) & tt_score_mask);

    TT_Record record{};
    record.move.bits = MoveBits((payload >> tt_move_shift) & tt_move_mask);
    record.score     = std::bit_cast<std::int16_t>(packed_score);
    record.depth     = std::uint8_t((payload >> tt_depth_shift) & tt_depth_mask);
    record.age       = std::uint8_t((payload >> tt_age_shift) & tt_age_mask);
    record.flag      = TT_Flag((payload >> tt_flag_shift) & tt_flag_mask);
    return record;
}

[[nodiscard]] std::uint64_t make_signature(PositionKey zkey, std::uint64_t payload) {
    return zkey ^ payload ^ tt_signature_salt;
}

[[nodiscard]] PositionKey recover_key(std::uint64_t signature, std::uint64_t payload) {
    return signature ^ payload ^ tt_signature_salt;
}

[[nodiscard]] std::optional<TT_Snapshot> load_snapshot(const TT_Entry& entry) {
    const std::uint64_t signature_before = entry.signature.load(std::memory_order_acquire);
    const std::uint64_t payload          = entry.payload.load(std::memory_order_relaxed);
    const std::uint64_t signature_after  = entry.signature.load(std::memory_order_acquire);
    if (signature_before != signature_after)
        return std::nullopt;

    TT_Record record = unpack_payload(payload);
    if (!record.is_valid())
        return std::nullopt;

    return TT_Snapshot{.key = recover_key(signature_after, payload), .record = record};
}

void clear_entry(TT_Entry& entry) {
    entry.payload.store(0, std::memory_order_relaxed);
    entry.signature.store(0, std::memory_order_relaxed);
}
} // namespace

TT_Table tt{};

TT_Table::TT_Table() {
    resize(default_mb);
}

std::optional<TT_Record> TT_Table::probe(PositionKey zkey) const {
    const TT_Cluster& cluster = table[cluster_key(zkey)];

    for (const TT_Entry& entry : cluster.entries) {
        auto snapshot = load_snapshot(entry);
        if (snapshot && snapshot->key == zkey)
            return snapshot->record;
    }

    return std::nullopt;
}

void TT_Table::store(
    PositionKey zkey, Move move, EvalValue score, int depth, TT_Flag flag, int ply) {
    assert(depth >= 0 && depth <= std::numeric_limits<std::uint8_t>::max());
    assert(score >= std::numeric_limits<std::int16_t>::min() &&
           score <= std::numeric_limits<std::int16_t>::max());

    TT_Cluster& cluster = table[cluster_key(zkey)];

    // convert mate from root score into mate from current position
    if (score >= eval_value::tt_mate_bound)
        score += ply;
    else if (score <= -eval_value::tt_mate_bound)
        score -= ply;
    assert(score >= std::numeric_limits<std::int16_t>::min() &&
           score <= std::numeric_limits<std::int16_t>::max());

    // replacement policy: prefer same key, then lowest replacement score
    TT_Entry* target = &cluster.entries[0];
    TT_Record target_record{};
    int       target_replacement_score = std::numeric_limits<int>::max();
    bool      target_is_same_key       = false;

    for (TT_Entry& entry : cluster.entries) {
        auto snapshot = load_snapshot(entry);
        if (snapshot && snapshot->key == zkey) {
            if (flag != TT_Flag::Exact && depth + 2 < int(snapshot->record.depth))
                return;

            target             = &entry;
            target_record      = snapshot->record;
            target_is_same_key = true;
            break;
        }

        const int replacement_score =
            snapshot ? snapshot->record.replacement_score(age) : std::numeric_limits<int>::min();
        if (!target_is_same_key && replacement_score < target_replacement_score) {
            target                   = &entry;
            target_replacement_score = replacement_score;
        }
    }

    if (target_is_same_key && move.is_null())
        move = target_record.move;

    const TT_Record record{
        .move  = move,
        .score = std::int16_t(score),
        .depth = std::uint8_t(depth),
        .age   = age,
        .flag  = flag,
    };

    const std::uint64_t payload   = pack_payload(record);
    const std::uint64_t signature = make_signature(zkey, payload);

    target->payload.store(payload, std::memory_order_relaxed);
    target->signature.store(signature, std::memory_order_release);
}

void TT_Table::clear() {
    for (std::size_t i = 0; i < length; ++i) {
        for (auto& entry : table[i].entries)
            clear_entry(entry);
    }
    age = 0;
}

void TT_Table::resize(size_t mb) {
    if (mb == 0)
        mb = 1;

    std::uint64_t bytes = mb << 20;

    length = bytes / sizeof(TT_Cluster);
    length = 1ULL << std::bit_width(length - 1);

    table = std::make_unique<TT_Cluster[]>(length);
    shift = 64 - std::countr_zero(length);
    age   = 0;
}
