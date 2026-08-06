#include "search/tt.hpp"

#include <array>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace search {

namespace {
constexpr std::uint64_t tt_cluster_multiplier = 0x9e3779b97f4a7c15ull;
constexpr std::uint32_t one_mb_cluster_shift  = 50;

struct TTExpectedSnapshot {
    Move      move;
    EvalValue score;
    int       depth;
    TTBound   bound;
};

[[nodiscard]] bool
matches_expected_snapshot(const TTRecord&                          record,
                          const std::array<TTExpectedSnapshot, 4>& expected_snapshots) {
    for (const auto& expected : expected_snapshots) {
        if (record.move == expected.move && record.score == expected.score
            && record.depth == expected.depth && record.bound == expected.bound) {
            return true;
        }
    }

    return false;
}

[[nodiscard]] std::uint64_t cluster_index_for_one_mb_table(PositionKey zkey) {
    return (zkey * tt_cluster_multiplier) >> one_mb_cluster_shift;
}

[[nodiscard]] PositionKey find_key_in_same_one_mb_cluster(PositionKey zkey) {
    const std::uint64_t target_index = cluster_index_for_one_mb_table(zkey);

    for (std::uint64_t offset = 1; offset < 2'000'000; ++offset) {
        const PositionKey candidate = zkey + offset;
        if (cluster_index_for_one_mb_table(candidate) == target_index)
            return candidate;
    }

    return zkey;
}

[[nodiscard]] std::vector<PositionKey> find_keys_in_same_one_mb_cluster(PositionKey zkey,
                                                                        size_t      count) {
    const std::uint64_t      target_index = cluster_index_for_one_mb_table(zkey);
    std::vector<PositionKey> keys{zkey};

    for (std::uint64_t offset = 1; offset < 2'000'000 && keys.size() < count; ++offset) {
        const PositionKey candidate = zkey + offset;
        if (cluster_index_for_one_mb_table(candidate) == target_index)
            keys.push_back(candidate);
    }

    return keys;
}

void expect_record(PositionKey zkey,
                   Move        expected_move,
                   int         expected_score,
                   int         expected_depth,
                   TTBound     expected_bound) {
    auto entry = tt.probe(zkey);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(expected_move, entry->move);
    EXPECT_EQ(expected_score, entry->score);
    EXPECT_EQ(expected_depth, entry->depth);
    EXPECT_EQ(expected_bound, entry->bound);
}
} // namespace

class TTTest : public ::testing::Test {
protected:
    PositionKey key   = 0x123456789ABCDEF;
    Move        move  = Move(Square::A2, Square::A4);
    EvalValue   score = 100;
    int         depth = 5;
    TTBound     bound = TTBound::Exact;

    void SetUp() override { tt.clear(); }
};

TEST_F(TTTest, StoreAndProbe) {
    tt.store(key, move, score, depth, bound, 0);

    expect_record(key, move, score, depth, bound);
}

TEST_F(TTTest, StoredFieldBoundariesRoundTrip) {
    for (int i = 0; i < std::numeric_limits<std::uint8_t>::max(); ++i)
        tt.advance_generation();

    Move packed_move;
    packed_move.bits = std::numeric_limits<MoveBits>::max();

    constexpr std::int16_t packed_score = -12345;
    constexpr int          packed_depth = engine::max_search_ply;
    constexpr TTBound      packed_bound = TTBound::UpperBound;

    tt.store(key, packed_move, packed_score, packed_depth, packed_bound, 0);

    auto entry = tt.probe(key);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(packed_move, entry->move);
    EXPECT_EQ(packed_score, entry->score);
    EXPECT_EQ(packed_depth, entry->depth);
    EXPECT_EQ(std::numeric_limits<std::uint8_t>::max(), entry->generation);
    EXPECT_EQ(packed_bound, entry->bound);
}

TEST_F(TTTest, ClearRemovesEntries) {
    tt.store(key, move, score, depth, bound, 0);
    tt.clear();

    EXPECT_FALSE(tt.probe(key).has_value());
}

TEST_F(TTTest, MateScoresRoundTripThroughStorage) {
    struct Case {
        EvalValue root_score;
        int       ply;
        EvalValue stored_score;
    };

    const std::array cases{
        Case{.root_score = eval_value::mate - 5, .ply = 2, .stored_score = eval_value::mate - 3},
        Case{.root_score = -eval_value::mate + 6, .ply = 5, .stored_score = -eval_value::mate + 1},
    };

    for (const auto& tc : cases) {
        tt.clear();
        tt.store(key, move, tc.root_score, depth, bound, tc.ply);

        auto entry = tt.probe(key);
        ASSERT_TRUE(entry.has_value());
        EXPECT_EQ(tc.stored_score, entry->score);
        EXPECT_EQ(tc.root_score, entry->score_at_ply(tc.ply));
    }
}

TEST_F(TTTest, RecordCanCutoffWithSufficientDepthAndMatchingBound) {
    TTRecord record{
        .move       = move,
        .score      = std::int16_t(score),
        .depth      = std::uint8_t{5},
        .generation = 0,
        .bound      = TTBound::Exact,
    };

    EXPECT_TRUE(record.can_cutoff(0, 5, -10, 10));
    EXPECT_FALSE(record.can_cutoff(0, 6, -10, 10));

    record.bound = TTBound::LowerBound;
    EXPECT_TRUE(record.can_cutoff(10, 5, -10, 10));
    EXPECT_FALSE(record.can_cutoff(9, 5, -10, 10));

    record.bound = TTBound::UpperBound;
    EXPECT_TRUE(record.can_cutoff(-10, 5, -10, 10));
    EXPECT_FALSE(record.can_cutoff(-9, 5, -10, 10));

    record.bound = TTBound::None;
    EXPECT_FALSE(record.can_cutoff(0, 5, -10, 10));
}

TEST_F(TTTest, BoundForWindowClassifiesSearchResult) {
    EXPECT_EQ(tt_bound_for_window(10, -10, 10), TTBound::LowerBound);
    EXPECT_EQ(tt_bound_for_window(-10, -10, 10), TTBound::UpperBound);
    EXPECT_EQ(tt_bound_for_window(0, -10, 10), TTBound::Exact);
}

TEST_F(TTTest, ResizeTable) {
    tt.store(key, move, score, depth, bound, 0);
    EXPECT_TRUE(tt.probe(key).has_value());

    tt.resize(8);
    EXPECT_FALSE(tt.probe(key).has_value());

    tt.store(key, move, score, depth, bound, 0);
    expect_record(key, move, score, depth, bound);
}

TEST_F(TTTest, ResizeZeroKeepsUsableTable) {
    tt.resize(0);

    tt.store(key, move, score, depth, bound, 0);
    auto entry = tt.probe(key);

    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(move, entry->move);
}

TEST_F(TTTest, ResizeRoundsCapacityDownToLargestFittingPowerOfTwo) {
    struct Case {
        size_t megabytes;
        size_t expected_capacity_mb;
    };

    constexpr std::array cases{
        Case{.megabytes = 1, .expected_capacity_mb = 1},
        Case{.megabytes = 2, .expected_capacity_mb = 2},
        Case{.megabytes = 3, .expected_capacity_mb = 2},
        Case{.megabytes = 5, .expected_capacity_mb = 4},
    };

    TranspositionTable table;
    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.megabytes);
        table.resize(tc.megabytes);
        EXPECT_EQ(tc.expected_capacity_mb, table.capacity_mb());
    }
}

TEST_F(TTTest, ProbeRejectsDifferentFullKeyInSameCluster) {
    tt.resize(1);

    const PositionKey key1 = key;
    const PositionKey key2 = find_key_in_same_one_mb_cluster(key1);

    ASSERT_NE(key1, key2);
    ASSERT_EQ(cluster_index_for_one_mb_table(key1), cluster_index_for_one_mb_table(key2));

    tt.store(key1, move, score, depth, bound, 0);

    ASSERT_TRUE(tt.probe(key1).has_value());
    EXPECT_FALSE(tt.probe(key2).has_value());
}

TEST_F(TTTest, ReplacementScoreUsesDepthMinusWrappedAgeDistance) {
    TTRecord record{
        .move       = move,
        .score      = std::int16_t(score),
        .depth      = std::uint8_t(depth),
        .generation = std::uint8_t{255},
        .bound      = bound,
    };

    EXPECT_EQ(record.replacement_score(255), depth);
    EXPECT_EQ(record.replacement_score(0), depth - 4);

    TTRecord deeper_record = record;
    deeper_record.depth    = std::uint8_t(depth + 3);
    EXPECT_GT(deeper_record.replacement_score(0), record.replacement_score(0));
}

TEST_F(TTTest, SameKeyNullMoveStorePreservesPreviousMoveAndUpdatesAcceptedFields) {
    tt.store(key, move, 100, 6, TTBound::Exact, 0);

    tt.store(key, NULL_MOVE, 250, 7, TTBound::LowerBound, 0);

    expect_record(key, move, 250, 7, TTBound::LowerBound);
}

TEST_F(TTTest, SameKeyReplacementUsesDepthAndBoundQuality) {
    struct Case {
        const char* name;
        int         old_depth;
        TTBound     old_bound;
        int         new_depth;
        TTBound     new_bound;
        bool        replaces;
    };

    constexpr std::array cases{
        Case{"much shallower non-exact", 10, TTBound::Exact, 7, TTBound::LowerBound, false},
        Case{"shallower exact", 10, TTBound::LowerBound, 1, TTBound::Exact, true},
        Case{"similar-depth non-exact", 8, TTBound::Exact, 6, TTBound::UpperBound, true},
    };

    const Move new_move{Square::E2, Square::E4};
    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        tt.clear();
        tt.store(key, move, 100, tc.old_depth, tc.old_bound, 0);
        tt.store(key, new_move, 250, tc.new_depth, tc.new_bound, 0);

        if (tc.replaces)
            expect_record(key, new_move, 250, tc.new_depth, tc.new_bound);
        else
            expect_record(key, move, 100, tc.old_depth, tc.old_bound);
    }
}

TEST_F(TTTest, DifferentKeyNullMoveReplacementKeepsNullMove) {
    tt.resize(1);

    const auto keys = find_keys_in_same_one_mb_cluster(key, 5);
    ASSERT_EQ(5U, keys.size());

    const std::array<Move, 4> moves = {Move(A2, A3), Move(B2, B3), Move(C2, C3), Move(D2, D3)};

    for (size_t i = 0; i < moves.size(); ++i) {
        tt.store(keys[i], moves[i], score + int(i), int(i + 1), TTBound::Exact, 0);
        ASSERT_TRUE(tt.probe(keys[i]).has_value());
    }

    tt.store(keys[4], NULL_MOVE, 500, 8, TTBound::LowerBound, 0);

    expect_record(keys[4], NULL_MOVE, 500, 8, TTBound::LowerBound);
}

TEST_F(TTTest, FullClusterReplacementChoosesLowestReplacementScore) {
    tt.resize(1);

    const auto keys = find_keys_in_same_one_mb_cluster(key, 5);
    ASSERT_EQ(5U, keys.size());

    const std::array<int, 4>  depths = {9, 1, 5, 7};
    const std::array<Move, 5> moves  = {
        Move(A2, A3), Move(B2, B3), Move(C2, C3), Move(D2, D3), Move(E2, E3)};

    for (size_t i = 0; i < depths.size(); ++i) {
        tt.store(keys[i], moves[i], score + int(i), depths[i], TTBound::Exact, 0);
        ASSERT_TRUE(tt.probe(keys[i]).has_value());
    }

    tt.store(keys[4], moves[4], 500, 8, TTBound::LowerBound, 0);

    EXPECT_TRUE(tt.probe(keys[0]).has_value());
    EXPECT_FALSE(tt.probe(keys[1]).has_value());
    EXPECT_TRUE(tt.probe(keys[2]).has_value());
    EXPECT_TRUE(tt.probe(keys[3]).has_value());

    expect_record(keys[4], moves[4], 500, 8, TTBound::LowerBound);
}

TEST_F(TTTest, ProbeReturnsDetachedSnapshot) {
    tt.store(key, move, score, depth, bound, 0);

    auto first_probe = tt.probe(key);
    ASSERT_TRUE(first_probe.has_value());

    Move      new_move  = Move(Square::E2, Square::E4);
    EvalValue new_score = 200;
    tt.store(key, new_move, new_score, depth + 1, TTBound::LowerBound, 0);

    EXPECT_EQ(move, first_probe->move);
    EXPECT_EQ(score, first_probe->score);
    EXPECT_EQ(depth, first_probe->depth);
    EXPECT_EQ(bound, first_probe->bound);

    auto second_probe = tt.probe(key);
    ASSERT_TRUE(second_probe.has_value());
    EXPECT_EQ(new_move, second_probe->move);
    EXPECT_EQ(new_score, second_probe->score);
}

TEST_F(TTTest, StoreAndProbeRoundTripPublishedGeneration) {
    tt.advance_generation();
    tt.advance_generation();

    tt.store(key, move, score, depth, bound, 0);
    auto entry = tt.probe(key);

    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(std::uint8_t{2}, entry->generation);
    EXPECT_EQ(move, entry->move);
    EXPECT_EQ(score, entry->score);
    EXPECT_EQ(depth, entry->depth);
    EXPECT_EQ(bound, entry->bound);
}

TEST_F(TTTest, InvalidBoundsProbeAsMiss) {
    constexpr std::array bounds{TTBound::None, TTBound{255}};

    for (TTBound invalid_bound : bounds) {
        SCOPED_TRACE(std::to_underlying(invalid_bound));
        tt.clear();
        tt.store(key, move, score, depth, invalid_bound, 0);
        EXPECT_FALSE(tt.probe(key).has_value());
    }
}

TEST_F(TTTest, ConcurrentStoreAndProbeYieldOnlyCompleteSnapshots) {
    constexpr PositionKey                   shared_key = 0x0F0E0D0C0B0A0908ULL;
    const std::array<TTExpectedSnapshot, 4> expected_snapshots{{
        {Move(Square::A2, Square::A4), 111, 4, TTBound::Exact},
        {Move(Square::B2, Square::B4), -77, 6, TTBound::LowerBound},
        {Move(Square::C2, Square::C4), 205, 9, TTBound::UpperBound},
        {Move(Square::D2, Square::D4), 18, 12, TTBound::Exact},
    }};

    constexpr int writer_iterations = 20000;
    constexpr int reader_iterations = 30000;

    std::barrier     start_line(static_cast<std::ptrdiff_t>(expected_snapshots.size() + 2));
    std::atomic<int> hit_count{0};
    std::atomic<int> miss_count{0};
    std::atomic<int> invalid_snapshot_count{0};

    auto writer = [&](const TTExpectedSnapshot& snapshot) {
        start_line.arrive_and_wait();
        for (int i = 0; i < writer_iterations; ++i)
            tt.store(shared_key, snapshot.move, snapshot.score, snapshot.depth, snapshot.bound, 0);
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

} // namespace search
