#include <array>
#include <limits>

#include <gtest/gtest.h>

#include "search/time_manager.hpp"

namespace search {
namespace {

Limits clock_limits(Milliseconds::rep remaining, Milliseconds::rep increment = 0) {
    Limits limits;
    limits.set_wtime(remaining);
    limits.set_btime(remaining);
    limits.set_winc(increment);
    limits.set_binc(increment);
    return limits;
}

} // namespace

TEST(TimeManagerTest, UsesSideClockIncrementAndMovesToGo) {
    Limits limits = clock_limits(90000, 500);
    limits.set_btime(60000);
    limits.binc.reset();
    EXPECT_EQ(TimeManager(limits, WHITE).soft_limit(), Milliseconds{3450});
    EXPECT_EQ(TimeManager(limits, BLACK).soft_limit(), Milliseconds{1950});
    EXPECT_EQ(TimeManager(limits, WHITE).hard_limit(), Milliseconds{13800});
    limits.set_movestogo(10);
    EXPECT_EQ(TimeManager(limits, WHITE).soft_limit(), Milliseconds{9450});
    limits.set_movestogo(1);
    EXPECT_EQ(TimeManager(limits, WHITE).hard_limit(), Milliseconds{89950});
}

TEST(TimeManagerTest, IncrementCannotSpendTimeNotYetOnClock) {
    const TimeManager manager{clock_limits(1030, 3000), BLACK};
    EXPECT_EQ(manager.soft_limit(), Milliseconds{980});
    EXPECT_EQ(manager.hard_limit(), Milliseconds{980});
}

TEST(TimeManagerTest, ClockCapOverridesMinimumAndHugeIncrement) {
    using Rep              = Milliseconds::rep;
    constexpr Rep max_time = std::numeric_limits<Rep>::max();
    for (Rep remaining :
         {Rep{0}, Rep{1}, Rep{49}, Rep{50}, Rep{51}, Rep{60}, Rep{1030}, max_time}) {
        for (Rep increment : {Rep{0}, Rep{3000}, max_time}) {
            for (int moves : {1, 10, 30}) {
                SCOPED_TRACE(::testing::Message() << remaining << '/' << increment << '/' << moves);
                auto limits = clock_limits(remaining, increment);
                limits.set_movestogo(moves);
                TimeManager manager{limits, WHITE};
                const auto  cap = Milliseconds{remaining > 50 ? remaining - 50 : 0};
                const auto  expect_capped_budget = [&] {
                    ASSERT_TRUE(manager.soft_limit());
                    ASSERT_TRUE(manager.hard_limit());
                    EXPECT_GE(*manager.soft_limit(), Milliseconds{0});
                    EXPECT_LE(*manager.soft_limit(), *manager.hard_limit());
                    EXPECT_LE(*manager.hard_limit(), cap);
                };
                expect_capped_budget();
                manager.update_after_iteration(4, Move(E2, E4), 100);
                manager.update_after_iteration(5, Move(D2, D4), -100);
                expect_capped_budget();
                for (int depth = 6; depth < 16; ++depth)
                    manager.update_after_iteration(depth, Move(D2, D4), -100);
                expect_capped_budget();
            }
        }
    }
}

TEST(TimeManagerTest, WideClockAndIncrementSaturateSafely) {
    constexpr auto max_time = std::numeric_limits<Milliseconds::rep>::max();
    auto           limits   = clock_limits(max_time, max_time);
    limits.set_movestogo(1);
    TimeManager manager{limits, WHITE};
    EXPECT_EQ(manager.soft_limit(), Milliseconds{max_time - 50});
    EXPECT_EQ(manager.hard_limit(), Milliseconds{max_time - 50});
    manager.update_after_iteration(4, Move(E2, E4), 100);
    manager.update_after_iteration(5, Move(D2, D4), -100);
    EXPECT_EQ(manager.soft_limit(), Milliseconds{max_time - 50});
}

TEST(TimeManagerTest, ExplicitLimitsDisableFeedbackAndExtensions) {
    std::array<Limits, 4> requests;
    requests.fill(clock_limits(90000));
    requests[0].set_movetime(1234);
    requests[1].set_depth(10);
    requests[2].set_nodes(1000);
    requests[3].set_mate(2);
    for (size_t i = 0; i < requests.size(); ++i) {
        SCOPED_TRACE(i);
        TimeManager manager{requests[i], WHITE};
        EXPECT_FALSE(manager.soft_limit());
        const auto hard = i == 0 ? Milliseconds{1234} : Milliseconds{2950};
        EXPECT_EQ(manager.hard_limit(), hard);
        manager.update_after_iteration(4, Move(E2, E4), 100);
        manager.update_after_iteration(5, Move(D2, D4), -100);
        EXPECT_FALSE(manager.soft_limit());
        EXPECT_EQ(manager.hard_limit(), hard);
    }
}

TEST(TimeManagerTest, MovetimeOverridesEvenAnExhaustedClock) {
    auto limits = clock_limits(0);
    limits.set_movetime(1234);
    EXPECT_EQ(TimeManager(limits, WHITE).hard_limit(), Milliseconds{1234});
    EXPECT_FALSE(TimeManager(limits, WHITE).soft_limit());
}

TEST(TimeManagerTest, UntimedAndInfiniteRequestsHaveNoDeadline) {
    Limits limits;
    EXPECT_FALSE(TimeManager(limits, WHITE).hard_limit());
    limits.set_wtime(1000);
    EXPECT_FALSE(TimeManager(limits, WHITE).hard_limit());
    limits.set_btime(1000);
    limits.infinite = true;
    EXPECT_FALSE(TimeManager(limits, WHITE).hard_limit());
    EXPECT_FALSE(TimeManager(limits, WHITE).soft_limit());
}

TEST(TimeManagerTest, StabilitySavesTimeAndChangedFallingMoveRestoresIt) {
    TimeManager manager{clock_limits(31500), WHITE}; // Base target 1000 ms.
    for (int depth = 1; depth <= 3; ++depth) {
        manager.update_after_iteration(depth, Move(E2, E4), 100);
        EXPECT_EQ(manager.soft_limit(), Milliseconds{1000});
    }
    manager.update_after_iteration(4, Move(E2, E4), 100);
    EXPECT_EQ(manager.soft_limit(), Milliseconds{700});
    manager.update_after_iteration(5, Move(E2, E4), 100);
    manager.update_after_iteration(6, Move(E2, E4), 100);
    EXPECT_EQ(manager.soft_limit(), Milliseconds{500});
    manager.update_after_iteration(7, Move(D2, D4), -100);
    EXPECT_EQ(manager.soft_limit(), Milliseconds{2000});
    EXPECT_EQ(manager.hard_limit(), Milliseconds{4000});
    manager.update_after_iteration(8, Move(D2, D4), -100);
    EXPECT_EQ(manager.soft_limit(), Milliseconds{900});
}

TEST(TimeManagerTest, MateTransitionsDoNotBecomeCentipawnSwings) {
    struct ScoreTransition {
        EvalValue previous;
        EvalValue current;
    };
    constexpr std::array cases{
        ScoreTransition{eval_value::mate - 5, 100},
        ScoreTransition{100, -eval_value::mate + 5},
        ScoreTransition{eval_value::mate - 5, eval_value::mate - 9},
        ScoreTransition{-eval_value::mate + 5, 100},
    };

    for (const auto [previous, score] : cases) {
        SCOPED_TRACE(::testing::Message() << previous << " -> " << score);
        TimeManager manager{clock_limits(31500), WHITE};
        manager.update_after_iteration(4, Move(E2, E4), previous);
        manager.update_after_iteration(5, Move(E2, E4), score);
        EXPECT_EQ(manager.soft_limit(), Milliseconds{900});
    }
}

} // namespace search
