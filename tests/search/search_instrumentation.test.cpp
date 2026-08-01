#include "search/search_instrumentation.hpp"

#include <type_traits>

#include <gtest/gtest.h>

TEST(SearchInstrumentation, DisabledInstrumentationIsEmptyAndNoop) {
    static_assert(std::is_empty_v<SearchInstrumentation<false>>);

    SearchInstrumentation<false> stats;
    SearchInstrumentation<false> other;

    stats.node(1);
    stats.qnode(1);
    stats.beta_cutoff(1, 5);
    stats.pvs_research(1);
    stats.aspiration_fail_low();
    stats.aspiration_fail_high();
    stats.main_tt_probe(1);
    stats.main_tt_hit(1);
    stats.main_tt_cutoff(1);
    stats.q_tt_probe(1);
    stats.q_tt_hit(1);
    stats.q_tt_cutoff(1);
    stats.null_move_try(1);
    stats.null_move_cutoff(1);
    stats.razor_try(1);
    stats.razor_cutoff(1);
    stats.futility_skip(1);
    stats.lmr_try(1);
    stats.lmr_research(1);
    stats.quiet_cutoff(1);
    stats.quiet_malus_eligible_node(1);
    stats.quiet_malus_failed_quiet(1);
    stats.quiet_malus_update(1);
    stats.reset();
    stats += other;

    EXPECT_TRUE(stats.str().empty());
}

#if LATRUNCULI_SEARCH_STATS

TEST(SearchInstrumentation, RecordsRetainedEventFamiliesAndResets) {
    SearchInstrumentation<true> stats;

    constexpr int index = 1;
    stats.node(index);
    stats.qnode(index);
    stats.beta_cutoff(index, 1);
    stats.beta_cutoff(index, 2);
    stats.beta_cutoff(index, 4);
    stats.beta_cutoff(index, 5);
    stats.pvs_research(index);
    stats.aspiration_fail_low();
    stats.aspiration_fail_high();
    stats.main_tt_probe(index);
    stats.main_tt_hit(index);
    stats.main_tt_cutoff(index);
    stats.q_tt_probe(index);
    stats.q_tt_hit(index);
    stats.q_tt_cutoff(index);
    stats.null_move_try(index);
    stats.null_move_cutoff(index);
    stats.razor_try(index);
    stats.razor_cutoff(index);
    stats.futility_skip(index);
    stats.lmr_try(index);
    stats.lmr_research(index);
    stats.quiet_cutoff(index);
    stats.quiet_malus_eligible_node(index);
    stats.quiet_malus_failed_quiet(index);
    stats.quiet_malus_update(index);

    const auto& counters = stats.raw_counters();
    EXPECT_EQ(counters.nodes[index], 2);
    EXPECT_EQ(counters.qnodes[index], 1);
    EXPECT_EQ(counters.cutoff_index_sum[index], 12);
    EXPECT_EQ(counters.cutoff_index_1[index], 1);
    EXPECT_EQ(counters.cutoff_index_2[index], 1);
    EXPECT_EQ(counters.cutoff_index_3_4[index], 1);
    EXPECT_EQ(counters.cutoff_index_5_plus[index], 1);
    EXPECT_EQ(counters.pvs_researches[index], 1);
    EXPECT_EQ(counters.aspiration_fail_lows, 1);
    EXPECT_EQ(counters.aspiration_fail_highs, 1);
    EXPECT_EQ(counters.main_tt_probes[index], 1);
    EXPECT_EQ(counters.main_tt_hits[index], 1);
    EXPECT_EQ(counters.main_tt_cutoffs[index], 1);
    EXPECT_EQ(counters.q_tt_probes[index], 1);
    EXPECT_EQ(counters.q_tt_hits[index], 1);
    EXPECT_EQ(counters.q_tt_cutoffs[index], 1);
    EXPECT_EQ(counters.null_move_tries[index], 1);
    EXPECT_EQ(counters.null_move_cutoffs[index], 1);
    EXPECT_EQ(counters.razor_tries[index], 1);
    EXPECT_EQ(counters.razor_cutoffs[index], 1);
    EXPECT_EQ(counters.futility_skips[index], 1);
    EXPECT_EQ(counters.lmr_tries[index], 1);
    EXPECT_EQ(counters.lmr_researches[index], 1);
    EXPECT_EQ(counters.quiet_cutoffs[index], 1);
    EXPECT_EQ(counters.quiet_malus_eligible_nodes[index], 1);
    EXPECT_EQ(counters.quiet_malus_failed_quiets[index], 1);
    EXPECT_EQ(counters.quiet_malus_updates[index], 1);

    stats.reset();

    const auto& reset = stats.raw_counters();
    EXPECT_EQ(reset.nodes[index], 0);
    EXPECT_EQ(reset.cutoff_index_sum[index], 0);
    EXPECT_EQ(reset.main_tt_probes[index], 0);
    EXPECT_EQ(reset.null_move_tries[index], 0);
    EXPECT_EQ(reset.quiet_malus_updates[index], 0);
    EXPECT_EQ(reset.aspiration_fail_lows, 0);
    EXPECT_EQ(reset.aspiration_fail_highs, 0);
}

TEST(SearchInstrumentation, IgnoresOutOfRangeIndices) {
    SearchInstrumentation<true> stats;

    stats.node(-1);
    stats.node(engine::max_search_ply);
    stats.beta_cutoff(-1, 1);
    stats.beta_cutoff(engine::max_search_ply, 1);
    stats.quiet_malus_update(-1);
    stats.quiet_malus_update(engine::max_search_ply);

    const auto& counters = stats.raw_counters();
    EXPECT_EQ(counters.nodes.front(), 0);
    EXPECT_EQ(counters.nodes.back(), 0);
    EXPECT_EQ(counters.cutoff_index_1.front(), 0);
    EXPECT_EQ(counters.cutoff_index_1.back(), 0);
    EXPECT_EQ(counters.quiet_malus_updates.front(), 0);
    EXPECT_EQ(counters.quiet_malus_updates.back(), 0);
}

TEST(SearchInstrumentation, AggregatesCounters) {
    SearchCounters first;
    SearchCounters second;

    first.nodes[1]                      = 10;
    first.qnodes[1]                     = 4;
    first.cutoff_index_sum[1]           = 3;
    first.cutoff_index_1[1]             = 2;
    first.cutoff_index_2[1]             = 1;
    first.cutoff_index_3_4[1]           = 2;
    first.cutoff_index_5_plus[1]        = 3;
    first.pvs_researches[1]             = 1;
    first.main_tt_probes[1]             = 4;
    first.main_tt_hits[1]               = 3;
    first.main_tt_cutoffs[1]            = 2;
    first.q_tt_probes[1]                = 5;
    first.q_tt_hits[1]                  = 4;
    first.q_tt_cutoffs[1]               = 2;
    first.null_move_tries[1]            = 6;
    first.null_move_cutoffs[1]          = 3;
    first.razor_tries[1]                = 4;
    first.razor_cutoffs[1]              = 2;
    first.futility_skips[1]             = 7;
    first.lmr_tries[1]                  = 8;
    first.lmr_researches[1]             = 4;
    first.quiet_cutoffs[1]              = 3;
    first.quiet_malus_eligible_nodes[1] = 4;
    first.quiet_malus_failed_quiets[1]  = 5;
    first.quiet_malus_updates[1]        = 6;
    first.aspiration_fail_lows          = 1;
    first.aspiration_fail_highs         = 2;

    second.nodes[1]                      = 5;
    second.qnodes[1]                     = 3;
    second.cutoff_index_sum[1]           = 9;
    second.cutoff_index_1[1]             = 3;
    second.cutoff_index_2[1]             = 2;
    second.cutoff_index_3_4[1]           = 4;
    second.cutoff_index_5_plus[1]        = 6;
    second.pvs_researches[1]             = 2;
    second.main_tt_probes[1]             = 6;
    second.main_tt_hits[1]               = 4;
    second.main_tt_cutoffs[1]            = 3;
    second.q_tt_probes[1]                = 7;
    second.q_tt_hits[1]                  = 5;
    second.q_tt_cutoffs[1]               = 3;
    second.null_move_tries[1]            = 8;
    second.null_move_cutoffs[1]          = 5;
    second.razor_tries[1]                = 9;
    second.razor_cutoffs[1]              = 6;
    second.futility_skips[1]             = 11;
    second.lmr_tries[1]                  = 12;
    second.lmr_researches[1]             = 3;
    second.quiet_cutoffs[1]              = 7;
    second.quiet_malus_eligible_nodes[1] = 8;
    second.quiet_malus_failed_quiets[1]  = 9;
    second.quiet_malus_updates[1]        = 10;
    second.aspiration_fail_lows          = 4;
    second.aspiration_fail_highs         = 5;

    SearchInstrumentation<true> total{first};
    total += SearchInstrumentation<true>{second};

    const auto& counters = total.raw_counters();
    EXPECT_EQ(counters.nodes[1], 15);
    EXPECT_EQ(counters.qnodes[1], 7);
    EXPECT_EQ(counters.cutoff_index_sum[1], 12);
    EXPECT_EQ(counters.cutoff_index_1[1], 5);
    EXPECT_EQ(counters.cutoff_index_2[1], 3);
    EXPECT_EQ(counters.cutoff_index_3_4[1], 6);
    EXPECT_EQ(counters.cutoff_index_5_plus[1], 9);
    EXPECT_EQ(counters.pvs_researches[1], 3);
    EXPECT_EQ(counters.main_tt_probes[1], 10);
    EXPECT_EQ(counters.main_tt_hits[1], 7);
    EXPECT_EQ(counters.main_tt_cutoffs[1], 5);
    EXPECT_EQ(counters.q_tt_probes[1], 12);
    EXPECT_EQ(counters.q_tt_hits[1], 9);
    EXPECT_EQ(counters.q_tt_cutoffs[1], 5);
    EXPECT_EQ(counters.null_move_tries[1], 14);
    EXPECT_EQ(counters.null_move_cutoffs[1], 8);
    EXPECT_EQ(counters.razor_tries[1], 13);
    EXPECT_EQ(counters.razor_cutoffs[1], 8);
    EXPECT_EQ(counters.futility_skips[1], 18);
    EXPECT_EQ(counters.lmr_tries[1], 20);
    EXPECT_EQ(counters.lmr_researches[1], 7);
    EXPECT_EQ(counters.quiet_cutoffs[1], 10);
    EXPECT_EQ(counters.quiet_malus_eligible_nodes[1], 12);
    EXPECT_EQ(counters.quiet_malus_failed_quiets[1], 14);
    EXPECT_EQ(counters.quiet_malus_updates[1], 16);
    EXPECT_EQ(counters.aspiration_fail_lows, 5);
    EXPECT_EQ(counters.aspiration_fail_highs, 7);
}

TEST(SearchInstrumentation, FormatsStableDiagnostics) {
    SearchCounters counters;
    counters.aspiration_fail_lows          = 1;
    counters.aspiration_fail_highs         = 2;
    counters.nodes[1]                      = 100;
    counters.nodes[2]                      = 200;
    counters.qnodes[1]                     = 50;
    counters.qnodes[2]                     = 100;
    counters.cutoff_index_sum[1]           = 170;
    counters.cutoff_index_1[1]             = 40;
    counters.cutoff_index_2[1]             = 20;
    counters.cutoff_index_3_4[1]           = 10;
    counters.cutoff_index_5_plus[1]        = 10;
    counters.cutoff_index_sum[2]           = 325;
    counters.cutoff_index_1[2]             = 75;
    counters.cutoff_index_2[2]             = 35;
    counters.cutoff_index_3_4[2]           = 20;
    counters.cutoff_index_5_plus[2]        = 20;
    counters.pvs_researches[1]             = 7;
    counters.main_tt_probes[1]             = 60;
    counters.main_tt_hits[1]               = 30;
    counters.main_tt_cutoffs[1]            = 20;
    counters.q_tt_probes[1]                = 40;
    counters.q_tt_hits[1]                  = 10;
    counters.q_tt_cutoffs[1]               = 5;
    counters.null_move_tries[1]            = 6;
    counters.null_move_cutoffs[1]          = 3;
    counters.null_move_tries[2]            = 4;
    counters.null_move_cutoffs[2]          = 1;
    counters.razor_tries[1]                = 7;
    counters.razor_cutoffs[1]              = 2;
    counters.razor_tries[2]                = 3;
    counters.razor_cutoffs[2]              = 2;
    counters.futility_skips[1]             = 5;
    counters.futility_skips[2]             = 6;
    counters.lmr_tries[1]                  = 8;
    counters.lmr_researches[1]             = 2;
    counters.lmr_tries[2]                  = 12;
    counters.lmr_researches[2]             = 3;
    counters.quiet_cutoffs[4]              = 6;
    counters.quiet_malus_eligible_nodes[4] = 7;
    counters.quiet_malus_failed_quiets[4]  = 8;
    counters.quiet_malus_updates[4]        = 5;

    const SearchInstrumentation<true> stats{counters};

    EXPECT_EQ(stats.str(), R"(
Aspiration: fail-low=1 fail-high=2 re-searches=3
NullMove: tries=10 cutoffs=4 cutoff-rate=40.0%
RazorFutility: razor-tries=10 razor-cutoffs=4 razor-cutoff-rate=40.0% futility-skips=11
LMR: tries=20 re-searches=5 re-search-rate=25.0%
QuietHistory: quiet-cutoffs=6 malus-eligible=7 failed-quiets=8 malus-updates=5
 QH D |       Cutoffs |      Eligible |   FailedQuiet |   MalusUpdate
    4 |             6 |             7 |             8 |             5
  Ply |     Nodes (QNode%) |  Cutoffs (Early%/Late%) |      CutIdx Avg/1/2/3-4/5+% | PVS Re | MainTT Hit/Cut% |  QTT Hit/Cut% |   EBF / Cumul
    1 |       100 ( 50.0%) |       80 ( 50.0/ 50.0%) |  2.1 /  50.0/ 25.0/ 12.5/ 12.5% |      7 |  50.0/ 66.7% |  25.0/ 50.0% |   0.0 / 100.0
    2 |       200 ( 50.0%) |      150 ( 50.0/ 50.0%) |  2.2 /  50.0/ 23.3/ 13.3/ 13.3% |      0 |   0.0/  0.0% |   0.0/  0.0% |   2.0 /  14.1
)");
}

#endif
