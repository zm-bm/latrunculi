#include "search_stats.hpp"

#include <gtest/gtest.h>

TEST(SearchStats, AddMethodsAndReset) {
    SearchStats<true> stats;

    int ply = 1;
    stats.node(ply);
    stats.qnode(ply);
    stats.beta_cutoff(ply, true);
    stats.beta_cutoff(ply, false);
    stats.tt_probe(ply);
    stats.tt_hit(ply);
    stats.tt_cutoff(ply);

    EXPECT_EQ(stats.nodes[ply], 2);
    EXPECT_EQ(stats.qnodes[ply], 1);
    EXPECT_EQ(stats.cutoffs[ply], 2);
    EXPECT_EQ(stats.fail_high_early[ply], 1);
    EXPECT_EQ(stats.fail_high_late[ply], 1);
    EXPECT_EQ(stats.tt_probes[ply], 1);
    EXPECT_EQ(stats.tt_hits[ply], 1);
    EXPECT_EQ(stats.tt_cutoffs[ply], 1);

    stats.reset();

    EXPECT_EQ(stats.nodes[ply], 0);
    EXPECT_EQ(stats.qnodes[ply], 0);
    EXPECT_EQ(stats.cutoffs[ply], 0);
    EXPECT_EQ(stats.fail_high_early[ply], 0);
    EXPECT_EQ(stats.fail_high_late[ply], 0);
    EXPECT_EQ(stats.tt_probes[ply], 0);
    EXPECT_EQ(stats.tt_hits[ply], 0);
    EXPECT_EQ(stats.tt_cutoffs[ply], 0);
}

TEST(SearchStats, ArithmeticOperators) {
    SearchStats<true> stats1, stats2;
    stats1.nodes[1]  = 10;
    stats2.nodes[1]  = 5;
    stats1          += stats2;
    EXPECT_EQ(stats1.nodes[1], 15);
    auto stats3 = stats1 + stats2;
    EXPECT_EQ(stats3.nodes[1], 20);
}

TEST(SearchStats, Output) {
    SearchStats<true> stats;
    stats.nodes           = {0, 100, 200};
    stats.qnodes          = {0, 50, 100};
    stats.cutoffs         = {0, 80, 150};
    stats.fail_high_early = {0, 40, 75};
    stats.fail_high_late  = {0, 40, 75};
    stats.tt_probes       = {0, 60, 120};
    stats.tt_hits         = {0, 30, 90};
    stats.tt_cutoffs      = {0, 20, 60};

    std::ostringstream oss;
    std::print(oss, "{}", stats);

    EXPECT_NE(oss.str().find("Depth"), std::string::npos);
    EXPECT_NE(oss.str().find("Nodes"), std::string::npos);
    EXPECT_NE(oss.str().find("Cutoffs"), std::string::npos);
}
