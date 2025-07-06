#include "search_stats.hpp"

#include <gtest/gtest.h>

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

    std::ostringstream outputStream;
    outputStream << stats;

    EXPECT_NE(outputStream.str().find("Depth"), std::string::npos);
    EXPECT_NE(outputStream.str().find("Nodes"), std::string::npos);
    EXPECT_NE(outputStream.str().find("Cutoffs"), std::string::npos);
}
