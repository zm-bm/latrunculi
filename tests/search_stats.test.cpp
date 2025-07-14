#include "search_stats.hpp"

#include <gtest/gtest.h>

TEST(SearchStats, AddMethodsAndReset) {
    SearchStats<true> stats;
    int ply = 1;
    stats.addNode(ply);
    stats.addQNode(ply);
    stats.addBetaCutoff(ply, true);
    stats.addBetaCutoff(ply, false);
    stats.addTTProbe(ply);
    stats.addTTHit(ply);
    stats.addTTCutoff(ply);

    EXPECT_EQ(stats.nodes[ply], 2);
    EXPECT_EQ(stats.qNodes[ply], 1);
    EXPECT_EQ(stats.cutoffs[ply], 2);
    EXPECT_EQ(stats.failHighEarly[ply], 1);
    EXPECT_EQ(stats.failHighLate[ply], 1);
    EXPECT_EQ(stats.ttProbes[ply], 1);
    EXPECT_EQ(stats.ttHits[ply], 1);
    EXPECT_EQ(stats.ttCutoffs[ply], 1);

    stats.reset();

    EXPECT_EQ(stats.nodes[ply], 0);
    EXPECT_EQ(stats.qNodes[ply], 0);
    EXPECT_EQ(stats.cutoffs[ply], 0);
    EXPECT_EQ(stats.failHighEarly[ply], 0);
    EXPECT_EQ(stats.failHighLate[ply], 0);
    EXPECT_EQ(stats.ttProbes[ply], 0);
    EXPECT_EQ(stats.ttHits[ply], 0);
    EXPECT_EQ(stats.ttCutoffs[ply], 0);
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
    stats.nodes         = {0, 100, 200};
    stats.qNodes        = {0, 50, 100};
    stats.cutoffs       = {0, 80, 150};
    stats.failHighEarly = {0, 40, 75};
    stats.failHighLate  = {0, 40, 75};
    stats.ttProbes      = {0, 60, 120};
    stats.ttHits        = {0, 30, 90};
    stats.ttCutoffs     = {0, 20, 60};

    std::ostringstream oss;
    oss << stats;

    EXPECT_NE(oss.str().find("Depth"), std::string::npos);
    EXPECT_NE(oss.str().find("Nodes"), std::string::npos);
    EXPECT_NE(oss.str().find("Cutoffs"), std::string::npos);
}
