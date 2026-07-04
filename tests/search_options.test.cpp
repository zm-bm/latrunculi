#include "search_options.hpp"

#include <gtest/gtest.h>

#include "defs.hpp"

TEST(SearchOptionsTest, SettersApplyValidLimits) {
    SearchOptions opts;

    opts.set_depth(10);
    opts.set_movetime(2000);
    opts.set_nodes(10000);
    opts.set_wtime(3000);
    opts.set_btime(4000);
    opts.set_winc(12);
    opts.set_binc(13);
    opts.set_movestogo(5);

    EXPECT_EQ(opts.depth, 10);
    EXPECT_EQ(opts.movetime, 2000);
    EXPECT_EQ(opts.nodes, 10000);
    EXPECT_EQ(opts.wtime, 3000);
    EXPECT_EQ(opts.btime, 4000);
    EXPECT_EQ(opts.winc, 12);
    EXPECT_EQ(opts.binc, 13);
    EXPECT_EQ(opts.movestogo, 5);
}

TEST(SearchOptionsTest, SettersClampOutOfRangeValues) {
    SearchOptions opts;

    opts.set_depth(999);
    opts.set_movetime(-50);
    opts.set_nodes(-1);
    opts.set_wtime(-1);
    opts.set_btime(-1);
    opts.set_winc(-1);
    opts.set_binc(-1);
    opts.set_movestogo(0);

    EXPECT_EQ(opts.depth, MAX_SEARCH_DEPTH);
    EXPECT_EQ(opts.movetime, 1);
    EXPECT_EQ(opts.nodes, 0);
    EXPECT_EQ(opts.wtime, 0);
    EXPECT_EQ(opts.btime, 0);
    EXPECT_EQ(opts.winc, 0);
    EXPECT_EQ(opts.binc, 0);
    EXPECT_EQ(opts.movestogo, 1);
}

TEST(SearchOptionsTest, DefaultsLeaveOptionalLimitsUnset) {
    SearchOptions opts;

    EXPECT_EQ(opts.depth, MAX_SEARCH_DEPTH);
    EXPECT_EQ(opts.movetime, OPTION_NOT_SET);
    EXPECT_EQ(opts.nodes, OPTION_NOT_SET);
    EXPECT_EQ(opts.wtime, OPTION_NOT_SET);
    EXPECT_EQ(opts.btime, OPTION_NOT_SET);
    EXPECT_EQ(opts.winc, OPTION_NOT_SET);
    EXPECT_EQ(opts.binc, OPTION_NOT_SET);
    EXPECT_EQ(opts.movestogo, OPTION_NOT_SET);
}

TEST(SearchOptionsTest, MovetimeOverridesClockBudget) {
    SearchOptions opts;
    opts.set_movetime(1234);
    opts.set_wtime(90000);
    opts.set_btime(90000);
    opts.set_winc(500);
    opts.set_binc(500);

    EXPECT_EQ(opts.calc_searchtime_ms(WHITE), 1234);
}

TEST(SearchOptionsTest, ClockBudgetDefaultsMissingIncrementToZero) {
    SearchOptions opts;
    opts.set_wtime(90000);
    opts.set_btime(60000);
    opts.set_movestogo(30);

    EXPECT_EQ(opts.calc_searchtime_ms(WHITE), 2950);
    EXPECT_EQ(opts.calc_searchtime_ms(BLACK), 1950);
}

TEST(SearchOptionsTest, ClockBudgetUsesSideIncrement) {
    SearchOptions opts;
    opts.set_wtime(90000);
    opts.set_btime(60);
    opts.set_winc(500);
    opts.set_binc(100);
    opts.set_movestogo(30);

    EXPECT_EQ(opts.calc_searchtime_ms(WHITE), 3450);
    EXPECT_EQ(opts.calc_searchtime_ms(BLACK), 52);
}

TEST(SearchOptionsTest, ClockBudgetUsesMinimumWhenBudgetIsLow) {
    SearchOptions opts;
    opts.set_wtime(60);
    opts.set_btime(60);
    opts.set_movestogo(30);

    EXPECT_EQ(opts.calc_searchtime_ms(WHITE), 10);
    EXPECT_EQ(opts.calc_searchtime_ms(BLACK), 10);
}
