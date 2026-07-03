#include "search_instrumentation.hpp"

#include <format>
#include <sstream>

#include <gtest/gtest.h>

TEST(SearchInstrumentation, DisabledStatsAreNoOps) {
    SearchInstrumentation<false> stats;
    SearchInstrumentation<false> other;

    stats.node(1);
    stats.qnode(1);
    stats.beta_cutoff(1, 5);
    stats.pvs_research(1);
    stats.aspiration_fail_low();
    stats.aspiration_fail_high();
    stats.aspiration_research();
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
    stats.reset();
    stats += other;

    EXPECT_TRUE(std::format("{}", stats).empty());
    EXPECT_TRUE(stats.str().empty());
}

TEST(SearchInstrumentation, RecordsEventsAndReset) {
    SearchInstrumentation<true> stats;

    const int ply = 1;
    stats.node(ply);
    stats.qnode(ply);
    stats.beta_cutoff(ply, 1);
    stats.beta_cutoff(ply, 2);
    stats.beta_cutoff(ply, 4);
    stats.beta_cutoff(ply, 5);
    stats.pvs_research(ply);
    stats.aspiration_fail_low();
    stats.aspiration_fail_high();
    stats.aspiration_research();
    stats.main_tt_probe(ply);
    stats.main_tt_hit(ply);
    stats.main_tt_cutoff(ply);
    stats.q_tt_probe(ply);
    stats.q_tt_hit(ply);
    stats.q_tt_cutoff(ply);
    stats.null_move_try(ply);
    stats.null_move_cutoff(ply);
    stats.razor_try(ply);
    stats.razor_cutoff(ply);
    stats.futility_skip(ply);
    stats.lmr_try(ply);
    stats.lmr_research(ply);

    const auto& counters = stats.raw_counters();
    EXPECT_EQ(counters.nodes[ply], 2);
    EXPECT_EQ(counters.qnodes[ply], 1);
    EXPECT_EQ(counters.cutoffs[ply], 4);
    EXPECT_EQ(counters.fail_high_early[ply], 1);
    EXPECT_EQ(counters.fail_high_late[ply], 3);
    EXPECT_EQ(counters.cutoff_index_sum[ply], 12);
    EXPECT_EQ(counters.cutoff_index_1[ply], 1);
    EXPECT_EQ(counters.cutoff_index_2[ply], 1);
    EXPECT_EQ(counters.cutoff_index_3_4[ply], 1);
    EXPECT_EQ(counters.cutoff_index_5_plus[ply], 1);
    EXPECT_EQ(counters.pvs_researches[ply], 1);
    EXPECT_EQ(counters.aspiration_fail_lows, 1);
    EXPECT_EQ(counters.aspiration_fail_highs, 1);
    EXPECT_EQ(counters.aspiration_researches, 1);
    EXPECT_EQ(counters.main_tt_probes[ply], 1);
    EXPECT_EQ(counters.main_tt_hits[ply], 1);
    EXPECT_EQ(counters.main_tt_cutoffs[ply], 1);
    EXPECT_EQ(counters.q_tt_probes[ply], 1);
    EXPECT_EQ(counters.q_tt_hits[ply], 1);
    EXPECT_EQ(counters.q_tt_cutoffs[ply], 1);
    EXPECT_EQ(counters.null_move_tries[ply], 1);
    EXPECT_EQ(counters.null_move_cutoffs[ply], 1);
    EXPECT_EQ(counters.razor_tries[ply], 1);
    EXPECT_EQ(counters.razor_cutoffs[ply], 1);
    EXPECT_EQ(counters.futility_skips[ply], 1);
    EXPECT_EQ(counters.lmr_tries[ply], 1);
    EXPECT_EQ(counters.lmr_researches[ply], 1);

    stats.reset();

    const auto& reset_counters = stats.raw_counters();
    EXPECT_EQ(reset_counters.nodes[ply], 0);
    EXPECT_EQ(reset_counters.qnodes[ply], 0);
    EXPECT_EQ(reset_counters.cutoffs[ply], 0);
    EXPECT_EQ(reset_counters.fail_high_early[ply], 0);
    EXPECT_EQ(reset_counters.fail_high_late[ply], 0);
    EXPECT_EQ(reset_counters.cutoff_index_sum[ply], 0);
    EXPECT_EQ(reset_counters.cutoff_index_1[ply], 0);
    EXPECT_EQ(reset_counters.cutoff_index_2[ply], 0);
    EXPECT_EQ(reset_counters.cutoff_index_3_4[ply], 0);
    EXPECT_EQ(reset_counters.cutoff_index_5_plus[ply], 0);
    EXPECT_EQ(reset_counters.pvs_researches[ply], 0);
    EXPECT_EQ(reset_counters.aspiration_fail_lows, 0);
    EXPECT_EQ(reset_counters.aspiration_fail_highs, 0);
    EXPECT_EQ(reset_counters.aspiration_researches, 0);
    EXPECT_EQ(reset_counters.main_tt_probes[ply], 0);
    EXPECT_EQ(reset_counters.main_tt_hits[ply], 0);
    EXPECT_EQ(reset_counters.main_tt_cutoffs[ply], 0);
    EXPECT_EQ(reset_counters.q_tt_probes[ply], 0);
    EXPECT_EQ(reset_counters.q_tt_hits[ply], 0);
    EXPECT_EQ(reset_counters.q_tt_cutoffs[ply], 0);
    EXPECT_EQ(reset_counters.null_move_tries[ply], 0);
    EXPECT_EQ(reset_counters.null_move_cutoffs[ply], 0);
    EXPECT_EQ(reset_counters.razor_tries[ply], 0);
    EXPECT_EQ(reset_counters.razor_cutoffs[ply], 0);
    EXPECT_EQ(reset_counters.futility_skips[ply], 0);
    EXPECT_EQ(reset_counters.lmr_tries[ply], 0);
    EXPECT_EQ(reset_counters.lmr_researches[ply], 0);
}

TEST(SearchInstrumentation, ArithmeticOperators) {
    SearchCounters<true> counters1, counters2;

    counters1.nodes[1]              = 10;
    counters1.cutoffs[1]            = 2;
    counters1.cutoff_index_sum[1]   = 3;
    counters1.pvs_researches[1]     = 1;
    counters1.main_tt_probes[1]     = 4;
    counters1.q_tt_probes[1]        = 5;
    counters1.null_move_tries[1]    = 6;
    counters1.null_move_cutoffs[1]  = 3;
    counters1.razor_tries[1]        = 4;
    counters1.razor_cutoffs[1]      = 2;
    counters1.futility_skips[1]     = 7;
    counters1.lmr_tries[1]          = 8;
    counters1.lmr_researches[1]     = 4;
    counters1.aspiration_fail_lows  = 1;
    counters1.aspiration_fail_highs = 2;
    counters1.aspiration_researches = 3;
    counters2.nodes[1]              = 5;
    counters2.cutoffs[1]            = 3;
    counters2.cutoff_index_sum[1]   = 9;
    counters2.pvs_researches[1]     = 2;
    counters2.main_tt_probes[1]     = 6;
    counters2.q_tt_probes[1]        = 7;
    counters2.null_move_tries[1]    = 8;
    counters2.null_move_cutoffs[1]  = 5;
    counters2.razor_tries[1]        = 9;
    counters2.razor_cutoffs[1]      = 6;
    counters2.futility_skips[1]     = 11;
    counters2.lmr_tries[1]          = 12;
    counters2.lmr_researches[1]     = 3;
    counters2.aspiration_fail_lows  = 4;
    counters2.aspiration_fail_highs = 5;
    counters2.aspiration_researches = 6;

    SearchInstrumentation<true> stats1{counters1};
    SearchInstrumentation<true> stats2{counters2};
    stats1 += stats2;

    EXPECT_EQ(stats1.raw_counters().nodes[1], 15);
    EXPECT_EQ(stats1.raw_counters().cutoffs[1], 5);
    EXPECT_EQ(stats1.raw_counters().cutoff_index_sum[1], 12);
    EXPECT_EQ(stats1.raw_counters().pvs_researches[1], 3);
    EXPECT_EQ(stats1.raw_counters().main_tt_probes[1], 10);
    EXPECT_EQ(stats1.raw_counters().q_tt_probes[1], 12);
    EXPECT_EQ(stats1.raw_counters().null_move_tries[1], 14);
    EXPECT_EQ(stats1.raw_counters().null_move_cutoffs[1], 8);
    EXPECT_EQ(stats1.raw_counters().razor_tries[1], 13);
    EXPECT_EQ(stats1.raw_counters().razor_cutoffs[1], 8);
    EXPECT_EQ(stats1.raw_counters().futility_skips[1], 18);
    EXPECT_EQ(stats1.raw_counters().lmr_tries[1], 20);
    EXPECT_EQ(stats1.raw_counters().lmr_researches[1], 7);
    EXPECT_EQ(stats1.raw_counters().aspiration_fail_lows, 5);
    EXPECT_EQ(stats1.raw_counters().aspiration_fail_highs, 7);
    EXPECT_EQ(stats1.raw_counters().aspiration_researches, 9);

    auto stats3 = stats1 + stats2;
    EXPECT_EQ(stats3.raw_counters().nodes[1], 20);
    EXPECT_EQ(stats3.raw_counters().null_move_tries[1], 22);
    EXPECT_EQ(stats3.raw_counters().null_move_cutoffs[1], 13);
    EXPECT_EQ(stats3.raw_counters().razor_tries[1], 22);
    EXPECT_EQ(stats3.raw_counters().razor_cutoffs[1], 14);
    EXPECT_EQ(stats3.raw_counters().futility_skips[1], 29);
    EXPECT_EQ(stats3.raw_counters().lmr_tries[1], 32);
    EXPECT_EQ(stats3.raw_counters().lmr_researches[1], 10);
    EXPECT_EQ(stats3.raw_counters().aspiration_researches, 15);
}

TEST(SearchInstrumentation, Output) {
    SearchCounters<true> counters;
    counters.aspiration_fail_lows   = 1;
    counters.aspiration_fail_highs  = 2;
    counters.aspiration_researches  = 3;
    counters.nodes[1]               = 100;
    counters.nodes[2]               = 200;
    counters.qnodes[1]              = 50;
    counters.qnodes[2]              = 100;
    counters.cutoffs[1]             = 80;
    counters.cutoffs[2]             = 150;
    counters.fail_high_early[1]     = 40;
    counters.fail_high_early[2]     = 75;
    counters.fail_high_late[1]      = 40;
    counters.fail_high_late[2]      = 75;
    counters.cutoff_index_sum[1]    = 120;
    counters.cutoff_index_sum[2]    = 300;
    counters.cutoff_index_1[1]      = 40;
    counters.cutoff_index_2[1]      = 20;
    counters.cutoff_index_3_4[1]    = 10;
    counters.cutoff_index_5_plus[1] = 10;
    counters.pvs_researches[1]      = 7;
    counters.main_tt_probes[1]      = 60;
    counters.main_tt_hits[1]        = 30;
    counters.main_tt_cutoffs[1]     = 20;
    counters.q_tt_probes[1]         = 40;
    counters.q_tt_hits[1]           = 10;
    counters.q_tt_cutoffs[1]        = 5;
    counters.null_move_tries[1]     = 6;
    counters.null_move_cutoffs[1]   = 3;
    counters.null_move_tries[2]     = 4;
    counters.null_move_cutoffs[2]   = 1;
    counters.razor_tries[1]         = 7;
    counters.razor_cutoffs[1]       = 2;
    counters.razor_tries[2]         = 3;
    counters.razor_cutoffs[2]       = 2;
    counters.futility_skips[1]      = 5;
    counters.futility_skips[2]      = 6;
    counters.lmr_tries[1]           = 8;
    counters.lmr_researches[1]      = 2;
    counters.lmr_tries[2]           = 12;
    counters.lmr_researches[2]      = 3;

    SearchInstrumentation<true> stats{counters};
    std::ostringstream          oss;
    oss << std::format("{}", stats);

    EXPECT_EQ(stats.str(), std::format("{}", stats));
    EXPECT_NE(oss.str().find("Aspiration: fail-low=1 fail-high=2 re-searches=3"),
              std::string::npos);
    EXPECT_NE(oss.str().find("NullMove: tries=10 cutoffs=4 cutoff-rate=40.0%"), std::string::npos);
    EXPECT_NE(oss.str().find("RazorFutility: razor-tries=10 razor-cutoffs=4 "
                             "razor-cutoff-rate=40.0% futility-skips=11"),
              std::string::npos);
    EXPECT_NE(oss.str().find("LMR: tries=20 re-searches=5 re-search-rate=25.0%"),
              std::string::npos);
    EXPECT_NE(oss.str().find("Depth"), std::string::npos);
    EXPECT_NE(oss.str().find("Nodes"), std::string::npos);
    EXPECT_NE(oss.str().find("CutIdx"), std::string::npos);
    EXPECT_NE(oss.str().find("PVS Re"), std::string::npos);
    EXPECT_NE(oss.str().find("MainTT"), std::string::npos);
    EXPECT_NE(oss.str().find("QTT"), std::string::npos);
}
