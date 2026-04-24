#include "tt.hpp"

#include <bit>

TT_Table tt{};

TT_Table::TT_Table() {
    resize(default_mb);
}

std::optional<TT_Record> TT_Table::probe(uint64_t zkey) const {
    const TT_Cluster& cluster = table[cluster_key(zkey)];

    const uint16_t key = entry_key(zkey);
    for (const TT_Entry& entry : cluster.entries) {
        if (entry.key == key && entry.flag != TT_Flag::None) {
            // Read shared storage once and return a local snapshot. Callers must not retain
            // aliases into TT storage; later tasks will harden publication/validation semantics.
            return TT_Record{
                .move  = entry.move,
                .score = entry.score,
                .depth = entry.depth,
                .age   = entry.age,
                .flag  = entry.flag,
            };
        }
    }

    return std::nullopt;
}

void TT_Table::store(
    uint64_t zkey, Move move, int16_t score, uint8_t depth, TT_Flag flag, int ply) {
    TT_Cluster& cluster = table[cluster_key(zkey)];

    const uint16_t key = entry_key(zkey);

    // convert mate from root score into mate from current position
    if (score >= TT_MATE_BOUND)
        score += ply;
    else if (score <= -TT_MATE_BOUND)
        score -= ply;

    // replacement policy: prefer same key, then lowest replacement score
    TT_Entry* target = nullptr;
    for (TT_Entry& entry : cluster.entries) {
        if (entry.key == key) {
            target = &entry;
            break;
        } else if (!target || entry.replacement_score(age) < target->replacement_score(age)) {
            target = &entry;
        }
    }

    *target = {.move = move, .score = score, .key = key, .depth = depth, .age = age, .flag = flag};
}

void TT_Table::clear() {
    for (uint32_t i = 0; i < length; ++i) {
        for (auto& entry : table[i].entries) {
            entry = TT_Entry{};
        }
    }
    age = 0;
}

void TT_Table::resize(size_t mb) {
    uint64_t bytes = mb << 20;

    length = bytes / sizeof(TT_Cluster);
    length = 1ULL << std::bit_width(length - 1);

    table = std::make_unique<TT_Cluster[]>(length);
    shift = 64 - std::countr_zero(length);
    age   = 0;
}
