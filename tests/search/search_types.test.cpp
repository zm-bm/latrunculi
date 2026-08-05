#include <array>
#include <limits>
#include <string_view>

#include <gtest/gtest.h>

#include "search/principal_variation.hpp"
#include "search/root_line.hpp"
#include "search/search_limits.hpp"

namespace {

void expect_allocated_time(const SearchLimits& limits, Color side, Milliseconds expected) {
    auto allocated = limits.allocated_time(side);
    ASSERT_TRUE(allocated.has_value());
    EXPECT_EQ(*allocated, expected);
}

PrincipalVariation pv_for_move(Move move) {
    PrincipalVariation pv;
    PrincipalVariation child;
    pv.update(move, child);
    return pv;
}

PrincipalVariation pv_for_line(Move first, Move second) {
    PrincipalVariation child = pv_for_move(second);
    PrincipalVariation pv;
    pv.update(first, child);
    return pv;
}

RootLine completed_root_line(Move move, EvalValue value, int depth) {
    return RootLine{.root_move = move, .value = value, .depth = depth, .completed = true};
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
    SearchLimits        limits;

    limits.set_movetime(max_time);
    limits.set_nodes(max_nodes);

    EXPECT_EQ(limits.movetime, Milliseconds{max_time});
    EXPECT_EQ(limits.nodes, max_nodes);
}

TEST(SearchLimitsTest, SettersClampOutOfRangeValues) {
    SearchLimits limits;

    limits.set_depth(999);
    limits.set_movetime(-50);
    limits.set_wtime(-1);
    limits.set_btime(-1);
    limits.set_winc(-1);
    limits.set_binc(-1);
    limits.set_movestogo(0);
    limits.set_mate(0);

    EXPECT_EQ(limits.depth, SearchLimits::max_depth);
    EXPECT_EQ(limits.movetime, Milliseconds{1});
    EXPECT_EQ(limits.wtime, Milliseconds{0});
    EXPECT_EQ(limits.btime, Milliseconds{0});
    EXPECT_EQ(limits.winc, Milliseconds{0});
    EXPECT_EQ(limits.binc, Milliseconds{0});
    EXPECT_EQ(limits.movestogo, 1);
    EXPECT_EQ(limits.mate, 1);
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
    EXPECT_FALSE(limits.mate.has_value());
}

TEST(SearchLimitsTest, MateLimitUsesUciMoveDistanceForEitherSide) {
    SearchLimits limits;
    limits.set_mate(2);

    EXPECT_TRUE(limits.has_mate_within_limit(eval_value::mate - 3));
    EXPECT_TRUE(limits.has_mate_within_limit(-eval_value::mate + 4));
    EXPECT_FALSE(limits.has_mate_within_limit(eval_value::mate - 5));
    EXPECT_FALSE(limits.has_mate_within_limit(200));

    limits.mate.reset();
    EXPECT_FALSE(limits.has_mate_within_limit(eval_value::mate - 1));
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

TEST(SearchLimitsTest, ClockBudgetSaturatesWideTimeAndIncrement) {
    using Rep = Milliseconds::rep;

    constexpr Rep max_time = std::numeric_limits<Rep>::max();
    SearchLimits  limits;
    limits.set_wtime(max_time);
    limits.set_btime(0);
    limits.set_winc(max_time);
    limits.set_movestogo(1);

    expect_allocated_time(limits, WHITE, Milliseconds{max_time - 50});
}

TEST(PrincipalVariationTest, EqualityUsesActiveMoves) {
    EXPECT_EQ(pv_for_move(Move(E2, E4)), pv_for_move(Move(E2, E4)));
    EXPECT_EQ(pv_for_line(Move(E2, E4), Move(E7, E5)), pv_for_line(Move(E2, E4), Move(E7, E5)));
}

TEST(PrincipalVariationTest, EqualityRejectsDifferentLines) {
    EXPECT_NE(pv_for_move(Move(E2, E4)), pv_for_move(Move(D2, D4)));
    EXPECT_NE(pv_for_move(Move(E2, E4)), pv_for_line(Move(E2, E4), Move(E7, E5)));
}

TEST(PrincipalVariationTest, EqualityIgnoresInactiveStorage) {
    PrincipalVariation shortened = pv_for_line(Move(E2, E4), Move(E7, E5));
    shortened                    = pv_for_move(Move(E2, E4));

    EXPECT_EQ(shortened, pv_for_move(Move(E2, E4)));
}

TEST(RootLineTest, SelectsByDepthValueAndMoveBits) {
    struct Case {
        std::string_view        name;
        RootLine                fallback;
        std::array<RootLine, 2> candidates;
        Move                    expected_move;
    };

    const std::array cases{
        Case{
            .name          = "greater depth",
            .fallback      = completed_root_line(Move(E2, E4), 50, 2),
            .candidates    = {completed_root_line(Move(G1, F3), 0, 3),
                              completed_root_line(Move(D2, D4), 200, 1)},
            .expected_move = Move(G1, F3),
        },
        Case{
            .name          = "greater value at equal depth",
            .fallback      = completed_root_line(Move(E2, E4), 10, 3),
            .candidates    = {completed_root_line(Move(G1, F3), 25, 3),
                              completed_root_line(Move(D2, D4), 20, 3)},
            .expected_move = Move(G1, F3),
        },
        Case{
            .name          = "lower move bits at equal depth and value",
            .fallback      = completed_root_line(Move(H2, H3), 10, 3),
            .candidates    = {completed_root_line(Move(G2, G3), 10, 3),
                              completed_root_line(Move(A2, A3), 10, 3)},
            .expected_move = Move(A2, A3),
        },
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        const RootLine selected = select_best_root_line(tc.fallback, tc.candidates);
        EXPECT_EQ(selected.root_move, tc.expected_move);
    }
}

TEST(RootLineTest, PreservesFallbackWhenCandidatesAreUnusable) {
    const std::array candidates{
        RootLine{.root_move = Move(G1, F3), .value = 1000, .depth = 99, .completed = false},
        RootLine{.root_move = NULL_MOVE, .value = 1000, .depth = 99, .completed = true},
        RootLine{.root_move = Move(D2, D4), .value = 1000, .depth = 0, .completed = true},
    };

    const RootLine usable_fallback = completed_root_line(Move(E2, E4), 10, 2);
    EXPECT_EQ(select_best_root_line(usable_fallback, candidates), usable_fallback);

    const RootLine unusable_fallback{
        .root_move = NULL_MOVE, .value = eval_value::draw, .depth = 1, .completed = true};
    EXPECT_EQ(select_best_root_line(unusable_fallback, candidates), unusable_fallback);
}
