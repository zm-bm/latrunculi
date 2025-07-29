#include "search_options.hpp"

#include <gtest/gtest.h>

#include <sstream>

#include "defs.hpp"

TEST(SearchOptionsTest, ValidInput) {
    std::istringstream iss(
        "depth 10 movetime 2000 nodes 10000 wtime 3000 btime 4000 winc 12 binc 13 movestogo 5");
    SearchOptions opts(iss);

    EXPECT_EQ(opts.depth, 10);
    EXPECT_EQ(opts.movetime, 2000);
    EXPECT_EQ(opts.nodes, 10000);
    EXPECT_EQ(opts.wtime, 3000);
    EXPECT_EQ(opts.btime, 4000);
    EXPECT_EQ(opts.winc, 12);
    EXPECT_EQ(opts.binc, 13);
    EXPECT_EQ(opts.movestogo, 5);
}

TEST(SearchOptionsTest, InvalidNumericInput) {
    std::istringstream iss("depth abc");
    SearchOptions      opts(iss);
    EXPECT_EQ(opts.depth, MAX_DEPTH);
}

TEST(SearchOptionsTest, OutOfRangeValue) {
    std::istringstream iss("depth 999 movetime -50 movestogo 0");
    SearchOptions      opts(iss);

    EXPECT_EQ(opts.depth, MAX_DEPTH);
    EXPECT_EQ(opts.movetime, 1);
    EXPECT_EQ(opts.movestogo, 1);
}

TEST(SearchOptionsTest, MixedValidInvalidTokens) {
    std::istringstream iss("wtime 5000 randomtoken 1234 btime 6000 movestogo twenty");
    SearchOptions      opts(iss);

    EXPECT_EQ(opts.wtime, 5000);
    EXPECT_EQ(opts.btime, 6000);
    EXPECT_EQ(opts.movestogo, OPTION_NOT_SET);
}

TEST(SearchOptionsTest, ExtraTokensIgnored) {
    std::istringstream iss("depth 15 someextradata movetime 2500 invalidtoken");
    SearchOptions      opts(iss);

    EXPECT_EQ(opts.depth, 15);
    EXPECT_EQ(opts.movetime, 2500);
    EXPECT_EQ(opts.nodes, OPTION_NOT_SET);
}
