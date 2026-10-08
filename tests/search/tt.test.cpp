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
// One MiB contains 2^15 clusters, selected by the high 15 key bits.
constexpr std::uint32_t one_mb_cluster_shift = 49;

struct TTStoredFields {
    Move      move;
    EvalValue score;
    int       depth;
    TTBound   bound;
    EvalValue static_eval;
};

[[nodiscard]] std::uint64_t cluster_index_for_one_mb_table(PositionKey zkey) {
    return zkey >> one_mb_cluster_shift;
}

[[nodiscard]] std::vector<PositionKey> keys_in_same_one_mb_cluster(PositionKey zkey, size_t count) {
    const std::uint64_t      target_index = cluster_index_for_one_mb_table(zkey);
    std::vector<PositionKey> keys{zkey};

    for (std::uint64_t offset = 1; keys.size() < count; ++offset) {
        const PositionKey candidate = zkey + offset;
        if (cluster_index_for_one_mb_table(candidate) != target_index)
            break;
        keys.push_back(candidate);
    }

    return keys;
}

void expect_record(PositionKey zkey,
                   Move        expected_move,
                   int         expected_score,
                   int         expected_depth,
                   TTBound     expected_bound,
                   EvalValue   expected_static_eval = TTRecord::no_static_eval) {
    auto entry = tt.probe(zkey).record;
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(expected_move, entry->move);
    EXPECT_EQ(expected_score, entry->score);
    EXPECT_EQ(expected_depth, entry->depth);
    EXPECT_EQ(expected_bound, entry->bound);
    EXPECT_EQ(expected_static_eval, entry->static_eval);
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

TEST_F(TTTest, EntryAndClusterStorageShapeRemainsFixed) {
    EXPECT_EQ(10U, sizeof(TTEntry));
    EXPECT_EQ(32U, sizeof(TTCluster));
    EXPECT_EQ(32U, alignof(TTCluster));
    tt.resize(32);
    EXPECT_EQ(3'145'728U, tt.entry_count());
}

TEST_F(TTTest, StoredFieldBoundariesRoundTrip) {
    for (int i = 0; i < tt_generation_mask; ++i)
        tt.advance_generation();

    Move packed_move;
    packed_move.bits = std::numeric_limits<MoveBits>::max();

    constexpr std::int16_t packed_score       = -12345;
    constexpr int          packed_depth       = engine::max_search_depth;
    constexpr TTBound      packed_bound       = TTBound::UpperBound;
    constexpr EvalValue    packed_static_eval = std::numeric_limits<std::int16_t>::min();

    tt.store(key, packed_move, packed_score, packed_depth, packed_bound, 0, packed_static_eval);

    auto entry = tt.probe(key).record;
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(packed_move, entry->move);
    EXPECT_EQ(packed_score, entry->score);
    EXPECT_EQ(packed_depth, entry->depth);
    EXPECT_EQ(tt_generation_mask, entry->generation);
    EXPECT_EQ(packed_bound, entry->bound);
    EXPECT_TRUE(entry->has_static_eval());
    EXPECT_EQ(packed_static_eval, entry->static_eval);
}

TEST_F(TTTest, CompleteMetadataDomainRoundTrips) {
    constexpr std::array bounds{TTBound::Exact, TTBound::LowerBound, TTBound::UpperBound};

    for (int generation = 0; generation <= tt_generation_mask; ++generation) {
        for (int stored_depth = 0; stored_depth <= engine::max_search_depth; ++stored_depth) {
            for (TTBound stored_bound : bounds) {
                tt.store(key, move, score, stored_depth, stored_bound, 0, -1234);
                auto entry = tt.probe(key).record;
                ASSERT_TRUE(entry.has_value());
                EXPECT_EQ(stored_depth, entry->depth);
                EXPECT_EQ(generation, entry->generation);
                EXPECT_EQ(stored_bound, entry->bound);
                EXPECT_EQ(-1234, entry->static_eval);
            }
        }
        if (generation != tt_generation_mask)
            tt.advance_generation();
    }
}

TEST_F(TTTest, CompleteCacheableStaticEvaluationDomainRoundTrips) {
    for (int value = std::numeric_limits<std::int16_t>::min(); value < TTRecord::no_static_eval;
         ++value) {
        tt.store(key, move, score, depth, TTBound::Exact, 0, value);
        auto entry = tt.probe(key).record;
        ASSERT_TRUE(entry.has_value());
        ASSERT_TRUE(entry->has_static_eval());
        EXPECT_EQ(value, entry->static_eval);
    }

    for (EvalValue value : {EvalValue{TTRecord::no_static_eval},
                            EvalValue{std::numeric_limits<std::int16_t>::max()} + 1,
                            EvalValue{std::numeric_limits<std::int16_t>::min()} - 1}) {
        tt.clear();
        tt.store(key, move, score, depth, TTBound::Exact, 0, value);
        auto entry = tt.probe(key).record;
        ASSERT_TRUE(entry.has_value());
        EXPECT_FALSE(entry->has_static_eval());
    }
}

TEST_F(TTTest, ClearRemovesEntries) {
    tt.store(key, move, score, depth, bound, 0);
    tt.clear();

    EXPECT_FALSE(tt.probe(key).record.has_value());
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

        auto entry = tt.probe(key).record;
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
    EXPECT_TRUE(tt.probe(key).record.has_value());

    tt.resize(8);
    EXPECT_FALSE(tt.probe(key).record.has_value());

    tt.store(key, move, score, depth, bound, 0);
    expect_record(key, move, score, depth, bound);
}

TEST_F(TTTest, ResizeZeroKeepsUsableTable) {
    tt.resize(0);

    tt.store(key, move, score, depth, bound, 0);
    auto entry = tt.probe(key).record;

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

TEST_F(TTTest, ResizeAcrossAllocationSizesResetsEntriesAndGeneration) {
    constexpr std::array<PositionKey, 3> keys{
        0x0000000000001234ULL, 0x8000000000005678ULL, 0xffffffffffff9abcULL};

    TranspositionTable table;
    for (std::size_t megabytes : {1U, 2U, 3U, 32U, 0U, 2U}) {
        SCOPED_TRACE(megabytes);
        table.resize(megabytes);
        EXPECT_EQ(0, table.current_generation());
        table.advance_generation();

        for (PositionKey probe_key : keys) {
            EXPECT_FALSE(table.probe(probe_key).record.has_value());
            table.store(probe_key, move, score, depth, bound, 0, -17);

            const auto record = table.probe(probe_key).record;
            ASSERT_TRUE(record.has_value());
            EXPECT_EQ(move, record->move);
            EXPECT_EQ(score, record->score);
            EXPECT_EQ(depth, record->depth);
            EXPECT_EQ(bound, record->bound);
            EXPECT_EQ(-17, record->static_eval);
            EXPECT_EQ(1, record->generation);
        }
    }
}

TEST_F(TTTest, ProbeRejectsDifferentTagInSameCluster) {
    tt.resize(1);

    const PositionKey key1 = key;
    const PositionKey key2 = key1 + 1;

    ASSERT_NE(key1, key2);
    ASSERT_NE(std::uint16_t(key1), std::uint16_t(key2));
    ASSERT_EQ(cluster_index_for_one_mb_table(key1), cluster_index_for_one_mb_table(key2));

    tt.store(key1, move, score, depth, bound, 0);

    ASSERT_TRUE(tt.probe(key1).record.has_value());
    EXPECT_FALSE(tt.probe(key2).record.has_value());
}

TEST_F(TTTest, ReplacementBalancesDepthAndWrappedAge) {
    tt.resize(1);
    const auto keys = keys_in_same_one_mb_cluster(key, 4);
    ASSERT_EQ(4U, keys.size());

    for (int older_depth : {9, 10}) {
        SCOPED_TRACE(older_depth);
        tt.clear();
        for (int generation = 0; generation < tt_generation_mask; ++generation)
            tt.advance_generation();
        tt.store(keys[0], move, score, older_depth, TTBound::Exact, 0);
        tt.advance_generation();
        ASSERT_EQ(0, tt.current_generation());
        tt.store(keys[1], move, score, 5, TTBound::Exact, 0);
        tt.store(keys[2], move, score, 12, TTBound::Exact, 0);

        // One generation of age offsets four plies of depth; ties keep the first slot.
        const auto pending = tt.probe(keys[3]);
        ASSERT_FALSE(pending.record.has_value());
        tt.store(pending.writer, keys[3], move, score, 8, TTBound::Exact, 0);
        const std::size_t evicted = older_depth == 9 ? 0 : 1;
        for (std::size_t i = 0; i < keys.size(); ++i)
            EXPECT_EQ(tt.probe(keys[i]).record.has_value(), i != evicted);
    }
}

TEST_F(TTTest, SameKeyNullMoveStorePreservesPreviousMoveAndUpdatesAcceptedFields) {
    tt.store(key, move, 100, 6, TTBound::Exact, 0, -321);

    tt.store(key, NULL_MOVE, 250, 7, TTBound::LowerBound, 0);

    expect_record(key, move, 250, 7, TTBound::LowerBound, -321);
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
            expect_record(key, new_move, 100, tc.old_depth, tc.old_bound);
    }
}

TEST_F(TTTest, DifferentKeyNullMoveReplacementKeepsNullMove) {
    tt.resize(1);

    const auto keys = keys_in_same_one_mb_cluster(key, 4);
    ASSERT_EQ(4U, keys.size());

    const std::array<Move, 3> moves = {Move(A2, A3), Move(B2, B3), Move(C2, C3)};

    for (size_t i = 0; i < moves.size(); ++i) {
        tt.store(keys[i], moves[i], score + int(i), int(i + 1), TTBound::Exact, 0);
        ASSERT_TRUE(tt.probe(keys[i]).record.has_value());
    }

    tt.store(keys[3], NULL_MOVE, 500, 8, TTBound::LowerBound, 0);

    expect_record(keys[3], NULL_MOVE, 500, 8, TTBound::LowerBound);
}

TEST_F(TTTest, FullClusterReplacementChoosesLowestReplacementScore) {
    tt.resize(1);

    const auto keys = keys_in_same_one_mb_cluster(key, 4);
    ASSERT_EQ(4U, keys.size());

    const std::array<int, 3>  depths = {9, 1, 5};
    const std::array<Move, 4> moves  = {Move(A2, A3), Move(B2, B3), Move(C2, C3), Move(D2, D3)};

    for (size_t i = 0; i < depths.size(); ++i) {
        tt.store(keys[i], moves[i], score + int(i), depths[i], TTBound::Exact, 0);
        ASSERT_TRUE(tt.probe(keys[i]).record.has_value());
    }

    tt.store(keys[3], moves[3], 500, 8, TTBound::LowerBound, 0);

    EXPECT_TRUE(tt.probe(keys[0]).record.has_value());
    EXPECT_FALSE(tt.probe(keys[1]).record.has_value());
    EXPECT_TRUE(tt.probe(keys[2]).record.has_value());

    expect_record(keys[3], moves[3], 500, 8, TTBound::LowerBound);
}

TEST_F(TTTest, ProbeReturnsDetachedRecord) {
    tt.store(key, move, score, depth, bound, 0);

    auto first_probe = tt.probe(key).record;
    ASSERT_TRUE(first_probe.has_value());

    Move      new_move  = Move(Square::E2, Square::E4);
    EvalValue new_score = 200;
    tt.store(key, new_move, new_score, depth + 1, TTBound::LowerBound, 0);

    EXPECT_EQ(move, first_probe->move);
    EXPECT_EQ(score, first_probe->score);
    EXPECT_EQ(depth, first_probe->depth);
    EXPECT_EQ(bound, first_probe->bound);

    auto second_probe = tt.probe(key).record;
    ASSERT_TRUE(second_probe.has_value());
    EXPECT_EQ(new_move, second_probe->move);
    EXPECT_EQ(new_score, second_probe->score);
}

TEST_F(TTTest, StoreAndProbeRoundTripPublishedGeneration) {
    tt.advance_generation();
    tt.advance_generation();

    tt.store(key, move, score, depth, bound, 0);
    auto entry = tt.probe(key).record;

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
        EXPECT_FALSE(tt.probe(key).record.has_value());
    }
}

TEST_F(TTTest, ZeroTagAndZeroDepthAreValidWhenBoundIsPresent) {
    tt.clear();
    EXPECT_FALSE(tt.probe(0).record.has_value());
    tt.store(0, move, score, 0, TTBound::LowerBound, 0);
    expect_record(0, move, score, 0, TTBound::LowerBound);
    tt.clear();
    EXPECT_FALSE(tt.probe(0).record.has_value());
}

TEST_F(TTTest, MatchingTagsInTheSameClusterAlias) {
    tt.resize(1);
    const PositionKey alias = key ^ (PositionKey{1} << 20);
    ASSERT_NE(key, alias);
    ASSERT_EQ(cluster_index_for_one_mb_table(key), cluster_index_for_one_mb_table(alias));
    ASSERT_EQ(std::uint16_t(key), std::uint16_t(alias));
    tt.store(key, move, score, depth, bound, 0);
    expect_record(alias, move, score, depth, bound);
}

TEST_F(TTTest, WriterRechecksAReplacedSlotBeforePreservingHints) {
    tt.resize(1);
    const auto pending = tt.probe(key);
    tt.store(key + 1, move, 100, 64, TTBound::Exact, 0, 321);
    tt.store(pending.writer, key, NULL_MOVE, 50, 1, TTBound::LowerBound, 0);
    expect_record(key, NULL_MOVE, 50, 1, TTBound::LowerBound);
}

TEST_F(TTTest, WriterRechecksAnUpdatedSameTagDepth) {
    tt.store(key, move, 100, 1, TTBound::Exact, 0);
    const auto pending = tt.probe(key);
    tt.store(key, move, 200, 20, TTBound::Exact, 0);
    tt.store(pending.writer, key, NULL_MOVE, 50, 1, TTBound::LowerBound, 0);
    expect_record(key, move, 200, 20, TTBound::Exact);
}

TEST_F(TTTest, AgedBoundCanBeReplacedByAShallowResult) {
    tt.store(key, move, 100, 64, TTBound::Exact, 0);
    tt.advance_generation();
    tt.store(key, NULL_MOVE, 50, 1, TTBound::LowerBound, 0);
    expect_record(key, move, 50, 1, TTBound::LowerBound);
}

TEST_F(TTTest, ConcurrentRecordsContainOnlyStoredFieldValues) {
    for (bool different_keys : {false, true}) {
        tt.clear();
        constexpr PositionKey               shared_key = 0x0F0E0D0C0B0A0908ULL;
        const std::array<TTStoredFields, 4> stored_fields{{
            {Move(Square::A2, Square::A4), 111, 4, TTBound::Exact, -1200},
            {Move(Square::B2, Square::B4), -77, 6, TTBound::LowerBound, 2300},
            {Move(Square::C2, Square::C4), 205, 9, TTBound::UpperBound, -3400},
            {Move(Square::D2, Square::D4), 18, 12, TTBound::Exact, 4500},
        }};

        for (std::size_t i = 0; i < stored_fields.size(); ++i) {
            const auto& initial = stored_fields[i];
            tt.store(shared_key + (different_keys ? i : 0),
                     initial.move,
                     initial.score,
                     initial.depth,
                     initial.bound,
                     0,
                     initial.static_eval);
        }
        constexpr int writer_iterations = 20000;
        constexpr int reader_iterations = 30000;

        std::barrier     start_line(static_cast<std::ptrdiff_t>(stored_fields.size() + 2));
        std::atomic<int> hit_count{0};
        std::atomic<int> miss_count{0};
        std::atomic<int> invalid_record_count{0};

        auto writer = [&](std::size_t index) {
            const auto& fields = stored_fields[index];
            start_line.arrive_and_wait();
            for (int i = 0; i < writer_iterations; ++i)
                tt.store(shared_key + (different_keys ? index : 0),
                         fields.move,
                         fields.score,
                         fields.depth,
                         fields.bound,
                         0,
                         fields.static_eval);
        };

        auto reader = [&]() {
            start_line.arrive_and_wait();
            for (int i = 0; i < reader_iterations; ++i) {
                auto probe = tt.probe(shared_key + (different_keys ? i % 4 : 0)).record;
                if (!probe.has_value()) {
                    miss_count.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                hit_count.fetch_add(1, std::memory_order_relaxed);
                // Each field must come from a store; the combined record need not.
                const auto has_field = [&](auto member, auto value) {
                    for (const auto& expected : stored_fields)
                        if (expected.*member == value)
                            return true;
                    return false;
                };
                if (!has_field(&TTStoredFields::move, probe->move)
                    || !has_field(&TTStoredFields::score, probe->score)
                    || !has_field(&TTStoredFields::depth, probe->depth)
                    || !has_field(&TTStoredFields::bound, probe->bound)
                    || !has_field(&TTStoredFields::static_eval, probe->static_eval))
                    invalid_record_count.fetch_add(1, std::memory_order_relaxed);
            }
        };

        std::vector<std::jthread> workers;
        workers.reserve(stored_fields.size() + 2);
        for (std::size_t i = 0; i < stored_fields.size(); ++i)
            workers.emplace_back(writer, i);
        workers.emplace_back(reader);
        workers.emplace_back(reader);

        for (auto& worker : workers)
            worker.join();

        const int hits   = hit_count.load(std::memory_order_relaxed);
        const int misses = miss_count.load(std::memory_order_relaxed);

        EXPECT_GT(hits, 0);
        EXPECT_EQ(hits + misses, reader_iterations * 2);
        EXPECT_EQ(invalid_record_count.load(std::memory_order_relaxed), 0);

        // After the concurrent writers stop, establish and check one complete record.
        tt.store(shared_key, move, score, depth, TTBound::Exact, 0, -123);
        expect_record(shared_key, move, score, depth, TTBound::Exact, -123);
    }
}

} // namespace search
