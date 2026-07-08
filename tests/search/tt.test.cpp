#include "search/tt.hpp"

#include <array>
#include <atomic>
#include <barrier>
#include <limits>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace {
constexpr uint64_t tt_cluster_multiplier = 0x9e3779b97f4a7c15ull;
constexpr uint32_t one_mb_cluster_shift  = 50;

struct TT_ExpectedSnapshot {
    Move    move;
    int16_t score;
    uint8_t depth;
    TT_Flag flag;
};

[[nodiscard]] bool
matches_expected_snapshot(const TT_Record&                          record,
                          const std::array<TT_ExpectedSnapshot, 4>& expected_snapshots) {
    for (const auto& expected : expected_snapshots) {
        if (record.move == expected.move && record.score == expected.score &&
            record.depth == expected.depth && record.flag == expected.flag) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] uint64_t cluster_key_for_one_mb_table(uint64_t zkey) {
    return (zkey * tt_cluster_multiplier) >> one_mb_cluster_shift;
}

[[nodiscard]] uint64_t find_same_one_mb_cluster_key(uint64_t zkey) {
    const uint64_t target_cluster = cluster_key_for_one_mb_table(zkey);

    for (uint64_t offset = 1; offset < 2'000'000; ++offset) {
        const uint64_t candidate = zkey + offset;
        if (cluster_key_for_one_mb_table(candidate) == target_cluster)
            return candidate;
    }

    return zkey;
}

[[nodiscard]] std::vector<uint64_t> find_same_one_mb_cluster_keys(uint64_t zkey, size_t count) {
    const uint64_t        target_cluster = cluster_key_for_one_mb_table(zkey);
    std::vector<uint64_t> keys{zkey};

    for (uint64_t offset = 1; offset < 2'000'000 && keys.size() < count; ++offset) {
        const uint64_t candidate = zkey + offset;
        if (cluster_key_for_one_mb_table(candidate) == target_cluster)
            keys.push_back(candidate);
    }

    return keys;
}

void expect_record(uint64_t zkey,
                   Move     expected_move,
                   int      expected_score,
                   uint8_t  expected_depth,
                   TT_Flag  expected_flag) {
    auto entry = tt.probe(zkey);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(expected_move, entry->move);
    EXPECT_EQ(expected_score, entry->score);
    EXPECT_EQ(expected_depth, entry->depth);
    EXPECT_EQ(expected_flag, entry->flag);
}
} // namespace

class TT_Test : public ::testing::Test {
protected:
    uint64_t key   = 0x123456789ABCDEF;
    Move     move  = Move(Square::A2, Square::A4);
    int16_t  score = 100;
    uint8_t  depth = 5;
    TT_Flag  flag  = TT_Flag::Exact;

    void SetUp() override { tt.clear(); }
};

TEST_F(TT_Test, InitialState) {
    EXPECT_FALSE(tt.probe(0x123456789ABCDEF).has_value());
}

TEST_F(TT_Test, StoreAndProbe) {
    tt.store(key, move, score, depth, flag, 0);

    expect_record(key, move, score, depth, flag);
}

TEST_F(TT_Test, PackedFieldBoundariesRoundTrip) {
    for (int i = 0; i < std::numeric_limits<uint8_t>::max(); ++i)
        tt.age_table();

    Move packed_move;
    packed_move.bits = std::numeric_limits<uint16_t>::max();

    constexpr int16_t packed_score = -12345;
    constexpr uint8_t packed_depth = std::numeric_limits<uint8_t>::max();
    constexpr TT_Flag packed_flag  = TT_Flag::Upperbound;

    tt.store(key, packed_move, packed_score, packed_depth, packed_flag, 0);

    auto entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(packed_move, entry->move);
    EXPECT_EQ(packed_score, entry->score);
    EXPECT_EQ(packed_depth, entry->depth);
    EXPECT_EQ(std::numeric_limits<uint8_t>::max(), entry->age);
    EXPECT_EQ(packed_flag, entry->flag);
}

TEST_F(TT_Test, ClearRemovesEntries) {
    tt.store(key, move, score, depth, flag, 0);
    tt.clear();

    EXPECT_FALSE(tt.probe(key).has_value());
}

TEST_F(TT_Test, MateScoresRoundTripThroughStorage) {
    struct Case {
        int16_t root_score;
        int     ply;
        int16_t stored_score;
    };

    const std::array cases{
        Case{.root_score = eval_value::mate - 5, .ply = 2, .stored_score = eval_value::mate - 3},
        Case{.root_score = -eval_value::mate + 6, .ply = 5, .stored_score = -eval_value::mate + 1},
    };

    for (const auto& tc : cases) {
        tt.clear();
        tt.store(key, move, tc.root_score, depth, flag, tc.ply);

        auto entry = tt.probe(key);
        ASSERT_TRUE(entry.has_value());
        EXPECT_EQ(tc.stored_score, entry->score);
        EXPECT_EQ(tc.root_score, entry->score_at_ply(tc.ply));
    }
}

TEST_F(TT_Test, StoreSearchConvertsDepthAndMateScore) {
    constexpr int search_depth = 7;
    constexpr int ply          = 3;
    constexpr int mate_score   = eval_value::mate - 10;

    tt.store_search(key, move, mate_score, search_depth, TT_Flag::Exact, ply);

    auto entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(search_depth, entry->depth);
    EXPECT_EQ(mate_score + ply, entry->score);
    EXPECT_EQ(mate_score, entry->score_at_ply(ply));
}

TEST_F(TT_Test, RecordCanCutoffWithSufficientDepthAndMatchingBound) {
    TT_Record record{
        .move  = move,
        .score = score,
        .depth = 5,
        .age   = 0,
        .flag  = TT_Flag::Exact,
    };

    EXPECT_TRUE(record.can_cutoff(0, 5, -10, 10));
    EXPECT_FALSE(record.can_cutoff(0, 6, -10, 10));

    record.flag = TT_Flag::Lowerbound;
    EXPECT_TRUE(record.can_cutoff(10, 5, -10, 10));
    EXPECT_FALSE(record.can_cutoff(9, 5, -10, 10));

    record.flag = TT_Flag::Upperbound;
    EXPECT_TRUE(record.can_cutoff(-10, 5, -10, 10));
    EXPECT_FALSE(record.can_cutoff(-9, 5, -10, 10));

    record.flag = TT_Flag::None;
    EXPECT_FALSE(record.can_cutoff(0, 5, -10, 10));
}

TEST_F(TT_Test, BoundForWindowClassifiesSearchResult) {
    EXPECT_EQ(tt_bound_for_window(10, -10, 10), TT_Flag::Lowerbound);
    EXPECT_EQ(tt_bound_for_window(-10, -10, 10), TT_Flag::Upperbound);
    EXPECT_EQ(tt_bound_for_window(0, -10, 10), TT_Flag::Exact);
}

TEST_F(TT_Test, ResizeTable) {
    tt.store(key, move, score, depth, flag, 0);
    EXPECT_TRUE(tt.probe(key).has_value());

    tt.resize(8);
    EXPECT_FALSE(tt.probe(key).has_value());

    tt.store(key, move, score, depth, flag, 0);
    expect_record(key, move, score, depth, flag);
}

TEST_F(TT_Test, ResizeZeroKeepsUsableTable) {
    tt.resize(0);

    tt.store(key, move, score, depth, flag, 0);
    auto entry = tt.probe(key);

    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(move, entry->move);
    EXPECT_NE(nullptr, tt.prefetch_addr(key));
}

TEST_F(TT_Test, StoreAndProbeDifferentFullKeys) {
    uint64_t key2 = 0xFEDCBA9876543210;
    uint64_t key1 = 0x123456789ABC3210;

    tt.store(key1, move, score, depth, flag, 0);
    Move move2 = Move(Square::E2, Square::E4);
    tt.store(key2, move2, 200, 8, TT_Flag::Lowerbound, 0);

    expect_record(key1, move, score, depth, flag);
    expect_record(key2, move2, 200, 8, TT_Flag::Lowerbound);
}

TEST_F(TT_Test, ProbeRejectsDifferentFullKeyInSameCluster) {
    tt.resize(1);

    const uint64_t key1 = key;
    const uint64_t key2 = find_same_one_mb_cluster_key(key1);

    ASSERT_NE(key1, key2);
    ASSERT_EQ(cluster_key_for_one_mb_table(key1), cluster_key_for_one_mb_table(key2));

    tt.store(key1, move, score, depth, flag, 0);

    ASSERT_TRUE(tt.probe(key1).has_value());
    EXPECT_FALSE(tt.probe(key2).has_value());
}

TEST_F(TT_Test, ReplacementScoreUsesDepthMinusWrappedAgeDistance) {
    TT_Record record{
        .move  = move,
        .score = score,
        .depth = depth,
        .age   = uint8_t{255},
        .flag  = flag,
    };

    EXPECT_EQ(record.replacement_score(255), depth);
    EXPECT_EQ(record.replacement_score(0), depth - 4);

    TT_Record deeper_record = record;
    deeper_record.depth     = depth + 3;
    EXPECT_GT(deeper_record.replacement_score(0), record.replacement_score(0));
}

TEST_F(TT_Test, SameKeyNullMoveStorePreservesPreviousMoveAndUpdatesAcceptedFields) {
    tt.store(key, move, 100, 6, TT_Flag::Exact, 0);

    tt.store(key, NULL_MOVE, 250, 7, TT_Flag::Lowerbound, 0);

    expect_record(key, move, 250, 7, TT_Flag::Lowerbound);
}

TEST_F(TT_Test, SameKeyShallowNonExactStoreDoesNotReplaceMuchDeeperEntry) {
    tt.store(key, move, 100, 10, TT_Flag::Exact, 0);

    Move shallow_move{Square::E2, Square::E4};
    tt.store(key, shallow_move, 250, 7, TT_Flag::Lowerbound, 0);

    expect_record(key, move, 100, 10, TT_Flag::Exact);
}

TEST_F(TT_Test, SameKeyShallowExactStoreReplacesDeeperEntry) {
    tt.store(key, move, 100, 10, TT_Flag::Lowerbound, 0);

    Move exact_move{Square::E2, Square::E4};
    tt.store(key, exact_move, 250, 1, TT_Flag::Exact, 0);

    expect_record(key, exact_move, 250, 1, TT_Flag::Exact);
}

TEST_F(TT_Test, SameKeySimilarDepthNonExactStoreReplacesEntry) {
    tt.store(key, move, 100, 8, TT_Flag::Exact, 0);

    Move new_move{Square::E2, Square::E4};
    tt.store(key, new_move, 250, 6, TT_Flag::Upperbound, 0);

    expect_record(key, new_move, 250, 6, TT_Flag::Upperbound);
}

TEST_F(TT_Test, DifferentKeyNullMoveReplacementKeepsNullMove) {
    tt.resize(1);

    const auto keys = find_same_one_mb_cluster_keys(key, 5);
    ASSERT_EQ(5U, keys.size());

    const std::array<Move, 4> moves = {Move(A2, A3), Move(B2, B3), Move(C2, C3), Move(D2, D3)};

    for (size_t i = 0; i < moves.size(); ++i) {
        tt.store(keys[i], moves[i], score + int16_t(i), uint8_t(i + 1), TT_Flag::Exact, 0);
        ASSERT_TRUE(tt.probe(keys[i]).has_value());
    }

    tt.store(keys[4], NULL_MOVE, 500, 8, TT_Flag::Lowerbound, 0);

    expect_record(keys[4], NULL_MOVE, 500, 8, TT_Flag::Lowerbound);
}

TEST_F(TT_Test, FullClusterReplacementChoosesLowestReplacementScore) {
    tt.resize(1);

    const auto keys = find_same_one_mb_cluster_keys(key, 5);
    ASSERT_EQ(5U, keys.size());

    const std::array<uint8_t, 4> depths = {9, 1, 5, 7};
    const std::array<Move, 5>    moves  = {
        Move(A2, A3), Move(B2, B3), Move(C2, C3), Move(D2, D3), Move(E2, E3)};

    for (size_t i = 0; i < depths.size(); ++i) {
        tt.store(keys[i], moves[i], score + int16_t(i), depths[i], TT_Flag::Exact, 0);
        ASSERT_TRUE(tt.probe(keys[i]).has_value());
    }

    tt.store(keys[4], moves[4], 500, 8, TT_Flag::Lowerbound, 0);

    EXPECT_TRUE(tt.probe(keys[0]).has_value());
    EXPECT_FALSE(tt.probe(keys[1]).has_value());
    EXPECT_TRUE(tt.probe(keys[2]).has_value());
    EXPECT_TRUE(tt.probe(keys[3]).has_value());

    expect_record(keys[4], moves[4], 500, 8, TT_Flag::Lowerbound);
}

TEST_F(TT_Test, ProbeReturnsDetachedSnapshot) {
    tt.store(key, move, score, depth, flag, 0);

    auto first_probe = tt.probe(key);
    ASSERT_TRUE(first_probe.has_value());

    Move    new_move  = Move(Square::E2, Square::E4);
    int16_t new_score = 200;
    tt.store(key, new_move, new_score, depth + 1, TT_Flag::Lowerbound, 0);

    EXPECT_EQ(move, first_probe->move);
    EXPECT_EQ(score, first_probe->score);
    EXPECT_EQ(depth, first_probe->depth);
    EXPECT_EQ(flag, first_probe->flag);

    auto second_probe = tt.probe(key);
    ASSERT_TRUE(second_probe.has_value());
    EXPECT_EQ(new_move, second_probe->move);
    EXPECT_EQ(new_score, second_probe->score);
}

TEST_F(TT_Test, StoreAndProbeRoundTripPublishedAge) {
    tt.age_table();
    tt.age_table();

    tt.store(key, move, score, depth, flag, 0);
    auto entry = tt.probe(key);

    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(uint8_t{2}, entry->age);
    EXPECT_EQ(move, entry->move);
    EXPECT_EQ(score, entry->score);
    EXPECT_EQ(depth, entry->depth);
    EXPECT_EQ(flag, entry->flag);
}

TEST_F(TT_Test, NoneFlagEntriesProbeAsMiss) {
    tt.store(key, move, score, depth, TT_Flag::None, 0);
    EXPECT_FALSE(tt.probe(key).has_value());
}

TEST_F(TT_Test, InvalidFlagEntriesProbeAsMiss) {
    tt.store(key, move, score, depth, TT_Flag{255}, 0);
    EXPECT_FALSE(tt.probe(key).has_value());
}

TEST_F(TT_Test, ConcurrentStoreAndProbeYieldOnlyCompleteSnapshots) {
    constexpr uint64_t                       shared_key = 0x0F0E0D0C0B0A0908ULL;
    const std::array<TT_ExpectedSnapshot, 4> expected_snapshots{{
        {Move(Square::A2, Square::A4), 111, 4, TT_Flag::Exact},
        {Move(Square::B2, Square::B4), -77, 6, TT_Flag::Lowerbound},
        {Move(Square::C2, Square::C4), 205, 9, TT_Flag::Upperbound},
        {Move(Square::D2, Square::D4), 18, 12, TT_Flag::Exact},
    }};

    constexpr int writer_iterations = 20000;
    constexpr int reader_iterations = 30000;

    std::barrier     start_line(static_cast<std::ptrdiff_t>(expected_snapshots.size() + 2));
    std::atomic<int> hit_count{0};
    std::atomic<int> miss_count{0};
    std::atomic<int> invalid_snapshot_count{0};

    auto writer = [&](const TT_ExpectedSnapshot& snapshot) {
        start_line.arrive_and_wait();
        for (int i = 0; i < writer_iterations; ++i)
            tt.store(shared_key, snapshot.move, snapshot.score, snapshot.depth, snapshot.flag, 0);
    };

    auto reader = [&]() {
        start_line.arrive_and_wait();
        for (int i = 0; i < reader_iterations; ++i) {
            auto probe = tt.probe(shared_key);
            if (!probe.has_value()) {
                miss_count.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            hit_count.fetch_add(1, std::memory_order_relaxed);
            if (!matches_expected_snapshot(*probe, expected_snapshots))
                invalid_snapshot_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::jthread> workers;
    workers.reserve(expected_snapshots.size() + 2);
    for (const auto& snapshot : expected_snapshots)
        workers.emplace_back(writer, std::cref(snapshot));
    workers.emplace_back(reader);
    workers.emplace_back(reader);

    for (auto& worker : workers)
        worker.join();

    const int hits   = hit_count.load(std::memory_order_relaxed);
    const int misses = miss_count.load(std::memory_order_relaxed);

    EXPECT_GT(hits, 0);
    EXPECT_EQ(hits + misses, reader_iterations * 2);
    EXPECT_EQ(invalid_snapshot_count.load(std::memory_order_relaxed), 0);

    auto final_probe = tt.probe(shared_key);
    ASSERT_TRUE(final_probe.has_value());
    EXPECT_TRUE(matches_expected_snapshot(*final_probe, expected_snapshots));
}
