#include <limits>

#include <gtest/gtest.h>

#include "search/limits.hpp"

namespace search {

TEST(SearchLimitsTest, SettersApplyValidLimits) {
    Limits limits;

    limits.set_depth(10);
    limits.set_movetime(2000);
    limits.set_nodes(10000);
    limits.set_wtime(3000);
    limits.set_btime(4000);
    limits.set_winc(12);
    limits.set_binc(13);
    limits.set_movestogo(5);
    limits.set_mate(2);
    limits.set_root_moves({Move(E2, E4)});

    EXPECT_EQ(limits.depth, 10);
    EXPECT_EQ(limits.movetime, Milliseconds{2000});
    EXPECT_EQ(limits.nodes, 10000U);
    EXPECT_EQ(limits.wtime, Milliseconds{3000});
    EXPECT_EQ(limits.btime, Milliseconds{4000});
    EXPECT_EQ(limits.winc, Milliseconds{12});
    EXPECT_EQ(limits.binc, Milliseconds{13});
    EXPECT_EQ(limits.movestogo, 5);
    EXPECT_EQ(limits.mate, 2);
    EXPECT_EQ(limits.root_moves, std::vector{Move(E2, E4)});
}

TEST(SearchLimitsTest, SettersPreserveWideLimits) {
    using Rep = Milliseconds::rep;

    constexpr NodeCount max_nodes = std::numeric_limits<NodeCount>::max();
    constexpr Rep       max_time  = std::numeric_limits<Rep>::max();
    Limits              limits;

    limits.set_movetime(max_time);
    limits.set_nodes(max_nodes);

    EXPECT_EQ(limits.movetime, Milliseconds{max_time});
    EXPECT_EQ(limits.nodes, max_nodes);
}

TEST(SearchLimitsTest, SettersClampOutOfRangeValues) {
    Limits limits;

    limits.set_depth(999);
    limits.set_movetime(-50);
    limits.set_wtime(-1);
    limits.set_btime(-1);
    limits.set_winc(-1);
    limits.set_binc(-1);
    limits.set_movestogo(0);
    limits.set_mate(0);

    EXPECT_EQ(limits.depth, Limits::max_depth);
    EXPECT_EQ(limits.movetime, Milliseconds{1});
    EXPECT_EQ(limits.wtime, Milliseconds{0});
    EXPECT_EQ(limits.btime, Milliseconds{0});
    EXPECT_EQ(limits.winc, Milliseconds{0});
    EXPECT_EQ(limits.binc, Milliseconds{0});
    EXPECT_EQ(limits.movestogo, 1);
    EXPECT_EQ(limits.mate, 1);
}

TEST(SearchLimitsTest, DefaultsLeaveOptionalLimitsUnset) {
    Limits limits;

    EXPECT_EQ(limits.depth, Limits::max_depth);
    EXPECT_FALSE(limits.movetime.has_value());
    EXPECT_FALSE(limits.nodes.has_value());
    EXPECT_FALSE(limits.wtime.has_value());
    EXPECT_FALSE(limits.btime.has_value());
    EXPECT_FALSE(limits.winc.has_value());
    EXPECT_FALSE(limits.binc.has_value());
    EXPECT_FALSE(limits.movestogo.has_value());
    EXPECT_FALSE(limits.mate.has_value());
}

TEST(SearchLimitsTest, MateLimitUsesUciMoveDistanceForEitherSide) {
    Limits limits;
    limits.set_mate(2);

    EXPECT_TRUE(limits.has_mate_within_limit(eval_value::mate - 3));
    EXPECT_TRUE(limits.has_mate_within_limit(-eval_value::mate + 4));
    EXPECT_FALSE(limits.has_mate_within_limit(eval_value::mate - 5));
    EXPECT_FALSE(limits.has_mate_within_limit(200));

    limits.mate.reset();
    EXPECT_FALSE(limits.has_mate_within_limit(eval_value::mate - 1));
}

} // namespace search
