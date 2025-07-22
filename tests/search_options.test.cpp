#include "search_options.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "constants.hpp"
#include "types.hpp"

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
    EXPECT_TRUE(opts.warnings.empty());
}

TEST(SearchOptionsTest, InvalidNumericInput) {
    std::istringstream iss("depth abc");
    SearchOptions opts(iss);

    // Should remain unchanged from default
    EXPECT_EQ(opts.depth, MAX_DEPTH);
    ASSERT_FALSE(opts.warnings.empty());
    EXPECT_EQ(opts.warnings[0].name, "depth");
}

TEST(SearchOptionsTest, OutOfRangeValue) {
    std::istringstream iss("depth 100 movetime -50 movestogo 0");
    SearchOptions opts(iss);

    // depth, movetime, and movestogo are clamped
    EXPECT_EQ(opts.depth, MAX_DEPTH);
    EXPECT_EQ(opts.movetime, 1);
    EXPECT_EQ(opts.movestogo, 1);
    EXPECT_EQ(opts.warnings.size(), 3);

    // warnings should be recorded for each invalid input
    bool depthWarn = false, movetimeWarn = false, movestogoWarn = false;
    for (const auto &warn : opts.warnings) {
        if (warn.name == "depth") depthWarn = true;
        if (warn.name == "movetime") movetimeWarn = true;
        if (warn.name == "movestogo") movestogoWarn = true;
    }
    EXPECT_TRUE(depthWarn);
    EXPECT_TRUE(movetimeWarn);
    EXPECT_TRUE(movestogoWarn);
}

TEST(SearchOptionsTest, MixedValidInvalidTokens) {
    std::istringstream iss("wtime 5000 randomtoken 1234 btime 6000 movestogo twenty");
    SearchOptions opts(iss);

    // Valid tokens are parsed correctly
    EXPECT_EQ(opts.wtime, 5000);
    EXPECT_EQ(opts.btime, 6000);

    // movestogo receives an invalid string so should not be set
    EXPECT_EQ(opts.movestogo, OPTION_NOT_SET);

    bool movestogoWarn = false;
    for (const auto &warn : opts.warnings) {
        if (warn.name == "movestogo") movestogoWarn = true;
    }
    EXPECT_TRUE(movestogoWarn);
}

TEST(SearchOptionsTest, ExtraTokensIgnored) {
    std::istringstream iss("depth 15 someextradata movetime 2500 invalidtoken");
    SearchOptions opts(iss);

    // Valid tokens are parsed correctly, others values are not set
    EXPECT_EQ(opts.depth, 15);
    EXPECT_EQ(opts.movetime, 2500);
    EXPECT_EQ(opts.nodes, OPTION_NOT_SET);

    // Extra tokens should be ignored
    EXPECT_TRUE(opts.warnings.empty());
}
