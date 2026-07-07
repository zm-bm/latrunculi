#include "search/search_limits.hpp"

#include <gtest/gtest.h>

namespace {

void expect_allocated_time(const SearchLimits& limits, Color side, Milliseconds expected) {
    auto allocated = limits.allocated_time(side);
    ASSERT_TRUE(allocated.has_value());
    EXPECT_EQ(*allocated, expected);
}

} // namespace

TEST(SearchLimitsTest, SettersApplyValidLimits) {
    SearchLimits limits;

    limits.set_depth(10);
    limits.set_movetime(2000);
    limits.set_nodes(10000);
    limits.set_wtime(3000);
    limits.set_btime(4000);
    limits.set_winc(12);
    limits.set_binc(13);
    limits.set_movestogo(5);

    EXPECT_EQ(limits.depth, 10);
    EXPECT_EQ(limits.movetime, Milliseconds{2000});
    EXPECT_EQ(limits.nodes, 10000U);
    EXPECT_EQ(limits.wtime, Milliseconds{3000});
    EXPECT_EQ(limits.btime, Milliseconds{4000});
    EXPECT_EQ(limits.winc, Milliseconds{12});
    EXPECT_EQ(limits.binc, Milliseconds{13});
    EXPECT_EQ(limits.movestogo, 5);
}

TEST(SearchLimitsTest, SettersClampOutOfRangeValues) {
    SearchLimits limits;

    limits.set_depth(999);
    limits.set_movetime(-50);
    limits.set_nodes(-1);
    limits.set_wtime(-1);
    limits.set_btime(-1);
    limits.set_winc(-1);
    limits.set_binc(-1);
    limits.set_movestogo(0);

    EXPECT_EQ(limits.depth, SearchLimits::max_depth);
    EXPECT_EQ(limits.movetime, Milliseconds{1});
    EXPECT_EQ(limits.nodes, 0U);
    EXPECT_EQ(limits.wtime, Milliseconds{0});
    EXPECT_EQ(limits.btime, Milliseconds{0});
    EXPECT_EQ(limits.winc, Milliseconds{0});
    EXPECT_EQ(limits.binc, Milliseconds{0});
    EXPECT_EQ(limits.movestogo, 1);
}

TEST(SearchLimitsTest, DefaultsLeaveOptionalLimitsUnset) {
    SearchLimits limits;

    EXPECT_EQ(limits.depth, SearchLimits::max_depth);
    EXPECT_FALSE(limits.movetime.has_value());
    EXPECT_FALSE(limits.nodes.has_value());
    EXPECT_FALSE(limits.wtime.has_value());
    EXPECT_FALSE(limits.btime.has_value());
    EXPECT_FALSE(limits.winc.has_value());
    EXPECT_FALSE(limits.binc.has_value());
    EXPECT_FALSE(limits.movestogo.has_value());
}

TEST(SearchLimitsTest, MovetimeOverridesClockBudget) {
    SearchLimits limits;
    limits.set_movetime(1234);
    limits.set_wtime(90000);
    limits.set_btime(90000);
    limits.set_winc(500);
    limits.set_binc(500);

    expect_allocated_time(limits, WHITE, Milliseconds{1234});
}

TEST(SearchLimitsTest, ClockBudgetDefaultsMissingIncrementToZero) {
    SearchLimits limits;
    limits.set_wtime(90000);
    limits.set_btime(60000);
    limits.set_movestogo(30);

    expect_allocated_time(limits, WHITE, Milliseconds{2950});
    expect_allocated_time(limits, BLACK, Milliseconds{1950});
}

TEST(SearchLimitsTest, ClockBudgetUsesSideIncrement) {
    SearchLimits limits;
    limits.set_wtime(90000);
    limits.set_btime(60);
    limits.set_winc(500);
    limits.set_binc(100);
    limits.set_movestogo(30);

    expect_allocated_time(limits, WHITE, Milliseconds{3450});
    expect_allocated_time(limits, BLACK, Milliseconds{52});
}

TEST(SearchLimitsTest, ClockBudgetUsesMinimumWhenBudgetIsLow) {
    SearchLimits limits;
    limits.set_wtime(60);
    limits.set_btime(60);
    limits.set_movestogo(30);

    expect_allocated_time(limits, WHITE, Milliseconds{10});
    expect_allocated_time(limits, BLACK, Milliseconds{10});
}
