#include "search/instrumentation.hpp"

#include <type_traits>

#include <gtest/gtest.h>

namespace search {

TEST(Instrumentation, DisabledInstrumentationIsEmptyAndNoop) {
    static_assert(std::is_empty_v<Instrumentation<false>>);
    Instrumentation<false> stats;
    stats.node(1);
    stats.qnode(1);
    stats.beta_cutoff(1, 2);
    stats.quiet_malus_update(1);
    stats.reset();
    EXPECT_TRUE(stats.str().empty());
}

#if LATRUNCULI_SEARCH_STATS

TEST(Instrumentation, RecordsAndAggregatesEvents) {
    Instrumentation<true> first;
    first.node(1);
    first.qnode(2);
    first.beta_cutoff(1, 1);
    first.beta_cutoff(1, 4);
    first.main_tt_probe(1);
    first.main_tt_hit(1);
    first.quiet_malus_update(3);
    first.aspiration_fail_low();

    Instrumentation<true> second;
    second.node(1);
    second.beta_cutoff(2, 1);
    second.lmr_try(2);
    second.quiet_malus_update(4);
    second.aspiration_fail_high();
    first += second;

    const auto& c = first.raw_counters();
    EXPECT_EQ(c.nodes[1], 2);
    EXPECT_EQ(c.nodes[2], 1);
    EXPECT_EQ(c.qnodes[2], 1);
    EXPECT_EQ(c.beta_cutoffs, 3);
    EXPECT_EQ(c.first_move_cutoffs, 2);
    EXPECT_EQ(c.main_tt_probes[1], 1);
    EXPECT_EQ(c.main_tt_hits[1], 1);
    EXPECT_EQ(c.lmr_tries[2], 1);
    EXPECT_EQ(c.quiet_malus_updates, 2);
    EXPECT_EQ(c.aspiration_fail_lows, 1);
    EXPECT_EQ(c.aspiration_fail_highs, 1);
    EXPECT_NE(first.str().find("QuietHistory: quiet-cutoffs=0"), std::string::npos);
    EXPECT_EQ(first.str().find("EBF"), std::string::npos);

    first.reset();
    EXPECT_EQ(first.raw_counters().nodes[1], 0);
    EXPECT_EQ(first.raw_counters().quiet_malus_updates, 0);
}

TEST(Instrumentation, RejectsInvalidIndices) {
    Instrumentation<true> stats;
    stats.node(-1);
    stats.qnode(engine::max_search_ply);
    stats.beta_cutoff(1, 0);
    stats.beta_cutoff(engine::max_search_ply, 1);
    stats.quiet_malus_update(-1);
    EXPECT_EQ(stats.raw_counters().nodes[0], 0);
    EXPECT_EQ(stats.raw_counters().beta_cutoffs, 0);
    EXPECT_EQ(stats.raw_counters().quiet_malus_updates, 0);
}

#endif

} // namespace search
