#include "search_instrumentation.hpp"

#include <format>
#include <sstream>

#include <gtest/gtest.h>

#include "history.hpp"
#include "killers.hpp"
#include "test_util.hpp"

namespace {

MovePicker main_search_picker(Board& board, KillerMoves& killers, HistoryTable& history) {
    return MovePicker{{board, killers, history, 1, NULL_MOVE, NULL_MOVE},
                      MovePickerMode::MainSearch};
}

} // namespace

TEST(SearchInstrumentation, DisabledStatsAreNoOps) {
    TestBoard    board{STARTFEN};
    KillerMoves  killers;
    HistoryTable history;
    MovePicker   picker = main_search_picker(board, killers, history);

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
    stats.staged_node(1, picker);
    stats.staged_generation(1, picker);
    stats.staged_skip_quiet(1);
    stats.staged_cutoff(1, picker);
    stats.reset();
    stats += other;

    EXPECT_TRUE(std::format("{}", stats).empty());
    EXPECT_TRUE(stats.str().empty());
}

TEST(SearchInstrumentation, RecordsEventsAndReset) {
    TestBoard    board{STARTFEN};
    KillerMoves  killers;
    HistoryTable history;
    MovePicker   picker = main_search_picker(board, killers, history);

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
    stats.staged_node(ply, picker);
    ASSERT_FALSE(picker.next().is_null());
    stats.staged_generation(ply, picker);
    stats.staged_skip_quiet(ply);
    stats.staged_cutoff(ply, picker);

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
    EXPECT_EQ(counters.staged_nodes[ply], 1);
    EXPECT_EQ(counters.staged_noisy_generated_nodes[ply], 1);
    EXPECT_EQ(counters.staged_quiet_generated_nodes[ply], 1);
    EXPECT_EQ(counters.staged_skip_quiet_nodes[ply], 1);
    EXPECT_EQ(counters.staged_cutoff_quiet_nodes[ply], 1);

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
    EXPECT_EQ(reset_counters.staged_nodes[ply], 0);
    EXPECT_EQ(reset_counters.staged_noisy_generated_nodes[ply], 0);
    EXPECT_EQ(reset_counters.staged_quiet_generated_nodes[ply], 0);
    EXPECT_EQ(reset_counters.staged_skip_quiet_nodes[ply], 0);
    EXPECT_EQ(reset_counters.staged_cutoff_quiet_nodes[ply], 0);
}

TEST(SearchInstrumentation, ArithmeticOperators) {
    SearchCounters<true> counters1, counters2;

    counters1.nodes[1]                        = 10;
    counters1.cutoffs[1]                      = 2;
    counters1.cutoff_index_sum[1]             = 3;
    counters1.pvs_researches[1]               = 1;
    counters1.main_tt_probes[1]               = 4;
    counters1.q_tt_probes[1]                  = 5;
    counters1.staged_nodes[1]                 = 6;
    counters1.staged_noisy_generated_nodes[1] = 7;
    counters1.staged_quiet_generated_nodes[1] = 8;
    counters1.staged_skip_quiet_nodes[1]      = 9;
    counters1.aspiration_fail_lows            = 1;
    counters1.aspiration_fail_highs           = 2;
    counters1.aspiration_researches           = 3;
    counters2.nodes[1]                        = 5;
    counters2.cutoffs[1]                      = 3;
    counters2.cutoff_index_sum[1]             = 9;
    counters2.pvs_researches[1]               = 2;
    counters2.main_tt_probes[1]               = 6;
    counters2.q_tt_probes[1]                  = 7;
    counters2.staged_nodes[1]                 = 8;
    counters2.staged_noisy_generated_nodes[1] = 9;
    counters2.staged_quiet_generated_nodes[1] = 10;
    counters2.staged_skip_quiet_nodes[1]      = 11;
    counters2.aspiration_fail_lows            = 4;
    counters2.aspiration_fail_highs           = 5;
    counters2.aspiration_researches           = 6;

    SearchInstrumentation<true> stats1{counters1};
    SearchInstrumentation<true> stats2{counters2};
    stats1 += stats2;

    EXPECT_EQ(stats1.raw_counters().nodes[1], 15);
    EXPECT_EQ(stats1.raw_counters().cutoffs[1], 5);
    EXPECT_EQ(stats1.raw_counters().cutoff_index_sum[1], 12);
    EXPECT_EQ(stats1.raw_counters().pvs_researches[1], 3);
    EXPECT_EQ(stats1.raw_counters().main_tt_probes[1], 10);
    EXPECT_EQ(stats1.raw_counters().q_tt_probes[1], 12);
    EXPECT_EQ(stats1.raw_counters().staged_nodes[1], 14);
    EXPECT_EQ(stats1.raw_counters().staged_noisy_generated_nodes[1], 16);
    EXPECT_EQ(stats1.raw_counters().staged_quiet_generated_nodes[1], 18);
    EXPECT_EQ(stats1.raw_counters().staged_skip_quiet_nodes[1], 20);
    EXPECT_EQ(stats1.raw_counters().aspiration_fail_lows, 5);
    EXPECT_EQ(stats1.raw_counters().aspiration_fail_highs, 7);
    EXPECT_EQ(stats1.raw_counters().aspiration_researches, 9);

    auto stats3 = stats1 + stats2;
    EXPECT_EQ(stats3.raw_counters().nodes[1], 20);
    EXPECT_EQ(stats3.raw_counters().aspiration_researches, 15);
}

TEST(SearchInstrumentation, Output) {
    SearchCounters<true> counters;
    counters.aspiration_fail_lows                = 1;
    counters.aspiration_fail_highs               = 2;
    counters.aspiration_researches               = 3;
    counters.nodes[1]                            = 100;
    counters.nodes[2]                            = 200;
    counters.qnodes[1]                           = 50;
    counters.qnodes[2]                           = 100;
    counters.cutoffs[1]                          = 80;
    counters.cutoffs[2]                          = 150;
    counters.fail_high_early[1]                  = 40;
    counters.fail_high_early[2]                  = 75;
    counters.fail_high_late[1]                   = 40;
    counters.fail_high_late[2]                   = 75;
    counters.cutoff_index_sum[1]                 = 120;
    counters.cutoff_index_sum[2]                 = 300;
    counters.cutoff_index_1[1]                   = 40;
    counters.cutoff_index_2[1]                   = 20;
    counters.cutoff_index_3_4[1]                 = 10;
    counters.cutoff_index_5_plus[1]              = 10;
    counters.pvs_researches[1]                   = 7;
    counters.main_tt_probes[1]                   = 60;
    counters.main_tt_hits[1]                     = 30;
    counters.main_tt_cutoffs[1]                  = 20;
    counters.q_tt_probes[1]                      = 40;
    counters.q_tt_hits[1]                        = 10;
    counters.q_tt_cutoffs[1]                     = 5;
    counters.staged_nodes[1]                     = 10;
    counters.staged_noisy_generated_nodes[1]     = 9;
    counters.staged_quiet_generated_nodes[1]     = 7;
    counters.staged_skip_quiet_nodes[1]          = 3;
    counters.staged_cutoff_before_quiet_nodes[1] = 4;
    counters.staged_cutoff_quiet_nodes[1]        = 2;
    counters.staged_cutoff_bad_noisy_nodes[1]    = 1;

    SearchInstrumentation<true> stats{counters};
    std::ostringstream          oss;
    oss << std::format("{}", stats);

    EXPECT_EQ(stats.str(), std::format("{}", stats));
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
