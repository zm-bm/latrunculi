#pragma once

#include <optional>

#include "search/limits.hpp"

namespace search {

// Per-search timing policy. The main worker owns feedback and enforcement;
// callers suppress both limits while pondering.
class TimeManager {
public:
    TimeManager() = default;
    TimeManager(const Limits& limits, Color side);

    // Elapsed budgets from the common search start: nullopt disables a limit,
    // while zero expires immediately. A soft limit enables iteration feedback
    // and never exceeds the hard limit. Clock-derived budgets reserve overhead;
    // explicit movetime supplies only a hard limit, without that reserve.
    [[nodiscard]] std::optional<Milliseconds> hard_limit() const { return hard_budget; }
    [[nodiscard]] std::optional<Milliseconds> soft_limit() const { return soft_target; }

    // Supply the best move and root-side score once per completed aspiration
    // iteration. Partial results and bounds must not advance stability.
    // Does nothing when the soft limit is disabled.
    void update_after_iteration(int depth, Move best_move, EvalValue score);

private:
    std::optional<Milliseconds> hard_budget;
    std::optional<Milliseconds> soft_target;
    Milliseconds                base_target{0};
    Move                        previous_best_move{NULL_MOVE};
    EvalValue                   previous_score{0};
    int                         stable_move_iterations{0};
};

} // namespace search
