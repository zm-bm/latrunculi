#include "search_stats.hpp"

#include <format>
#include <sstream>

#include <gtest/gtest.h>

TEST(SearchStats, DisabledStatsAreNoOps) {
    SearchStats<false> stats;
    SearchStats<false> other;

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
    stats.staged_node(1);
    stats.staged_generation(1, true, true);
    stats.staged_skip_quiet(1);
    stats.staged_cutoff_before_quiet(1);
    stats.staged_cutoff_quiet(1);
    stats.staged_cutoff_bad_noisy(1);
    stats.reset();
    stats += other;

    EXPECT_TRUE(std::format("{}", stats).empty());
}

TEST(SearchStats, AddMethodsAndReset) {
    SearchStats<true> stats;

    int ply = 1;
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
    stats.staged_node(ply);
    stats.staged_generation(ply, true, true);
    stats.staged_skip_quiet(ply);
    stats.staged_cutoff_before_quiet(ply);
    stats.staged_cutoff_quiet(ply);
    stats.staged_cutoff_bad_noisy(ply);

    EXPECT_EQ(stats.nodes[ply], 2);
    EXPECT_EQ(stats.qnodes[ply], 1);
    EXPECT_EQ(stats.cutoffs[ply], 4);
    EXPECT_EQ(stats.fail_high_early[ply], 1);
    EXPECT_EQ(stats.fail_high_late[ply], 3);
    EXPECT_EQ(stats.cutoff_index_sum[ply], 12);
    EXPECT_EQ(stats.cutoff_index_1[ply], 1);
    EXPECT_EQ(stats.cutoff_index_2[ply], 1);
    EXPECT_EQ(stats.cutoff_index_3_4[ply], 1);
    EXPECT_EQ(stats.cutoff_index_5_plus[ply], 1);
    EXPECT_EQ(stats.pvs_researches[ply], 1);
    EXPECT_EQ(stats.aspiration_fail_lows, 1);
    EXPECT_EQ(stats.aspiration_fail_highs, 1);
    EXPECT_EQ(stats.aspiration_researches, 1);
    EXPECT_EQ(stats.main_tt_probes[ply], 1);
    EXPECT_EQ(stats.main_tt_hits[ply], 1);
    EXPECT_EQ(stats.main_tt_cutoffs[ply], 1);
    EXPECT_EQ(stats.q_tt_probes[ply], 1);
    EXPECT_EQ(stats.q_tt_hits[ply], 1);
    EXPECT_EQ(stats.q_tt_cutoffs[ply], 1);
    EXPECT_EQ(stats.staged_nodes[ply], 1);
    EXPECT_EQ(stats.staged_noisy_generated_nodes[ply], 1);
    EXPECT_EQ(stats.staged_quiet_generated_nodes[ply], 1);
    EXPECT_EQ(stats.staged_skip_quiet_nodes[ply], 1);
    EXPECT_EQ(stats.staged_cutoff_before_quiet_nodes[ply], 1);
    EXPECT_EQ(stats.staged_cutoff_quiet_nodes[ply], 1);
    EXPECT_EQ(stats.staged_cutoff_bad_noisy_nodes[ply], 1);

    stats.reset();

    EXPECT_EQ(stats.nodes[ply], 0);
    EXPECT_EQ(stats.qnodes[ply], 0);
    EXPECT_EQ(stats.cutoffs[ply], 0);
    EXPECT_EQ(stats.fail_high_early[ply], 0);
    EXPECT_EQ(stats.fail_high_late[ply], 0);
    EXPECT_EQ(stats.cutoff_index_sum[ply], 0);
    EXPECT_EQ(stats.cutoff_index_1[ply], 0);
    EXPECT_EQ(stats.cutoff_index_2[ply], 0);
    EXPECT_EQ(stats.cutoff_index_3_4[ply], 0);
    EXPECT_EQ(stats.cutoff_index_5_plus[ply], 0);
    EXPECT_EQ(stats.pvs_researches[ply], 0);
    EXPECT_EQ(stats.aspiration_fail_lows, 0);
    EXPECT_EQ(stats.aspiration_fail_highs, 0);
    EXPECT_EQ(stats.aspiration_researches, 0);
    EXPECT_EQ(stats.main_tt_probes[ply], 0);
    EXPECT_EQ(stats.main_tt_hits[ply], 0);
    EXPECT_EQ(stats.main_tt_cutoffs[ply], 0);
    EXPECT_EQ(stats.q_tt_probes[ply], 0);
    EXPECT_EQ(stats.q_tt_hits[ply], 0);
    EXPECT_EQ(stats.q_tt_cutoffs[ply], 0);
    EXPECT_EQ(stats.staged_nodes[ply], 0);
    EXPECT_EQ(stats.staged_noisy_generated_nodes[ply], 0);
    EXPECT_EQ(stats.staged_quiet_generated_nodes[ply], 0);
    EXPECT_EQ(stats.staged_skip_quiet_nodes[ply], 0);
    EXPECT_EQ(stats.staged_cutoff_before_quiet_nodes[ply], 0);
    EXPECT_EQ(stats.staged_cutoff_quiet_nodes[ply], 0);
    EXPECT_EQ(stats.staged_cutoff_bad_noisy_nodes[ply], 0);
}

TEST(SearchStats, ArithmeticOperators) {
    SearchStats<true> stats1, stats2;

    stats1.nodes[1]                         = 10;
    stats1.cutoffs[1]                       = 2;
    stats1.cutoff_index_sum[1]              = 3;
    stats1.pvs_researches[1]                = 1;
    stats1.main_tt_probes[1]                = 4;
    stats1.q_tt_probes[1]                   = 5;
    stats1.staged_nodes[1]                  = 6;
    stats1.staged_noisy_generated_nodes[1]  = 7;
    stats1.staged_quiet_generated_nodes[1]  = 8;
    stats1.staged_skip_quiet_nodes[1]       = 9;
    stats1.aspiration_fail_lows             = 1;
    stats1.aspiration_fail_highs            = 2;
    stats1.aspiration_researches            = 3;
    stats2.nodes[1]                         = 5;
    stats2.cutoffs[1]                       = 3;
    stats2.cutoff_index_sum[1]              = 9;
    stats2.pvs_researches[1]                = 2;
    stats2.main_tt_probes[1]                = 6;
    stats2.q_tt_probes[1]                   = 7;
    stats2.staged_nodes[1]                  = 8;
    stats2.staged_noisy_generated_nodes[1]  = 9;
    stats2.staged_quiet_generated_nodes[1]  = 10;
    stats2.staged_skip_quiet_nodes[1]       = 11;
    stats2.aspiration_fail_lows             = 4;
    stats2.aspiration_fail_highs            = 5;
    stats2.aspiration_researches            = 6;
    stats1                                 += stats2;

    EXPECT_EQ(stats1.nodes[1], 15);
    EXPECT_EQ(stats1.cutoffs[1], 5);
    EXPECT_EQ(stats1.cutoff_index_sum[1], 12);
    EXPECT_EQ(stats1.pvs_researches[1], 3);
    EXPECT_EQ(stats1.main_tt_probes[1], 10);
    EXPECT_EQ(stats1.q_tt_probes[1], 12);
    EXPECT_EQ(stats1.staged_nodes[1], 14);
    EXPECT_EQ(stats1.staged_noisy_generated_nodes[1], 16);
    EXPECT_EQ(stats1.staged_quiet_generated_nodes[1], 18);
    EXPECT_EQ(stats1.staged_skip_quiet_nodes[1], 20);
    EXPECT_EQ(stats1.aspiration_fail_lows, 5);
    EXPECT_EQ(stats1.aspiration_fail_highs, 7);
    EXPECT_EQ(stats1.aspiration_researches, 9);

    auto stats3 = stats1 + stats2;
    EXPECT_EQ(stats3.nodes[1], 20);
    EXPECT_EQ(stats3.aspiration_researches, 15);
}

TEST(SearchStats, Output) {
    SearchStats<true> stats;
    stats.aspiration_fail_lows                = 1;
    stats.aspiration_fail_highs               = 2;
    stats.aspiration_researches               = 3;
    stats.nodes[1]                            = 100;
    stats.nodes[2]                            = 200;
    stats.qnodes[1]                           = 50;
    stats.qnodes[2]                           = 100;
    stats.cutoffs[1]                          = 80;
    stats.cutoffs[2]                          = 150;
    stats.fail_high_early[1]                  = 40;
    stats.fail_high_early[2]                  = 75;
    stats.fail_high_late[1]                   = 40;
    stats.fail_high_late[2]                   = 75;
    stats.cutoff_index_sum[1]                 = 120;
    stats.cutoff_index_sum[2]                 = 300;
    stats.cutoff_index_1[1]                   = 40;
    stats.cutoff_index_2[1]                   = 20;
    stats.cutoff_index_3_4[1]                 = 10;
    stats.cutoff_index_5_plus[1]              = 10;
    stats.pvs_researches[1]                   = 7;
    stats.main_tt_probes[1]                   = 60;
    stats.main_tt_hits[1]                     = 30;
    stats.main_tt_cutoffs[1]                  = 20;
    stats.q_tt_probes[1]                      = 40;
    stats.q_tt_hits[1]                        = 10;
    stats.q_tt_cutoffs[1]                     = 5;
    stats.staged_nodes[1]                     = 10;
    stats.staged_noisy_generated_nodes[1]     = 9;
    stats.staged_quiet_generated_nodes[1]     = 7;
    stats.staged_skip_quiet_nodes[1]          = 3;
    stats.staged_cutoff_before_quiet_nodes[1] = 4;
    stats.staged_cutoff_quiet_nodes[1]        = 2;
    stats.staged_cutoff_bad_noisy_nodes[1]    = 1;

    std::ostringstream oss;
    oss << std::format("{}", stats);

    EXPECT_NE(oss.str().find("Aspiration: fail-low=1 fail-high=2 re-searches=3"),
              std::string::npos);
    EXPECT_NE(oss.str().find("StagedPicker: nodes=10 noisy-generated=9 (90.0%)"),
              std::string::npos);
    EXPECT_NE(oss.str().find("quiet-generated=7 (70.0%)"), std::string::npos);
    EXPECT_NE(oss.str().find("skip-quiet=3 (30.0%)"), std::string::npos);
    EXPECT_NE(oss.str().find("cutoff-before-quiet=4 (40.0%)"), std::string::npos);
    EXPECT_NE(oss.str().find("cutoff-quiet=2 (20.0%)"), std::string::npos);
    EXPECT_NE(oss.str().find("cutoff-bad-noisy=1 (10.0%)"), std::string::npos);
    EXPECT_NE(oss.str().find("Depth"), std::string::npos);
    EXPECT_NE(oss.str().find("Nodes"), std::string::npos);
    EXPECT_NE(oss.str().find("CutIdx"), std::string::npos);
    EXPECT_NE(oss.str().find("PVS Re"), std::string::npos);
    EXPECT_NE(oss.str().find("MainTT"), std::string::npos);
    EXPECT_NE(oss.str().find("QTT"), std::string::npos);
}
