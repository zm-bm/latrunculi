#include "tt.hpp"

TT_Table tt{};

TT_Table::TT_Table() { resize(DEFAULT_HASH_MB); }

TT_Entry* TT_Table::probe(U64 zkey) {
    TT_Cluster& cluster = table[cluster_key(zkey)];
    const uint16_t key  = entry_key(zkey);

    for (TT_Entry& e : cluster.entries) {
        if (e.key == key && e.flag != TT_Flag::None) return &e;
    }

    return nullptr;
}

void TT_Table::store(U64 zkey, Move move, I16 score, U8 depth, TT_Flag flag, int ply) {
    TT_Cluster& cluster = table[cluster_key(zkey)];
    const uint16_t key  = entry_key(zkey);

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
    for (U32 i = 0; i < length; ++i) {
        for (auto& entry : table[i].entries) {
            entry = TT_Entry{};
        }
    }
    age = 0;
}

void TT_Table::resize(size_t mb) {
    U64 bytes = U64(mb) << 20;

    // Ensure # of clusters is a power of two
    length = bytes / sizeof(TT_Cluster);
    length = 1ULL << std::bit_width(length - 1);

    table = std::make_unique<TT_Cluster[]>(length);
    shift = 64 - std::countr_zero(length);
    age   = 0;
}
