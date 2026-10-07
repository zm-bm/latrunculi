#include "search/time_manager.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>

namespace search {
namespace {

using TimeRep = Milliseconds::rep;

constexpr TimeRep MoveOverheadMs   = 50;
constexpr TimeRep MinimumTargetMs  = 10;
constexpr int     DefaultMovesToGo = 30;
constexpr int     HardLimitPercent = 400;

constexpr int FeedbackStartDepth       = 4;
constexpr int MaxStableMoveIterations  = 5;
constexpr int StabilityDiscountPercent = 10;
constexpr int ChangedMovePercent       = 150;
constexpr int MaxScoreDropPercent      = 50;
constexpr int ScoreDropCpPerPercent    = 2;

// Return min(floor(value * percent / 100), cap) without overflowing the
// intermediate product, even for the largest UCI clock values.
TimeRep scale_capped(TimeRep value, int percent, TimeRep cap) {
    assert(value >= 0 && percent > 0 && cap >= 0);

    const TimeRep whole = value / 100;
    const TimeRep part  = value % 100 * percent / 100;
    if (whole > cap / percent)
        return cap;
    const TimeRep scaled = whole * percent;
    return part > cap - scaled ? cap : scaled + part;
}

} // namespace

TimeManager::TimeManager(const Limits& limits, Color side) {
    if (limits.infinite)
        return;
    if (limits.movetime) {
        hard_budget = limits.movetime;
        return;
    }
    if (!limits.wtime || !limits.btime)
        return;

    constexpr TimeRep MaxTimeMs    = std::numeric_limits<TimeRep>::max();
    const TimeRep     remaining_ms = (side == WHITE ? *limits.wtime : *limits.btime).count();
    const TimeRep     increment_ms = std::max(
        TimeRep{0}, (side == WHITE ? limits.winc : limits.binc).value_or(Milliseconds{0}).count());
    const TimeRep clock_cap_ms = remaining_ms > MoveOverheadMs ? remaining_ms - MoveOverheadMs : 0;
    const int     moves_to_go  = std::max(limits.movestogo.value_or(DefaultMovesToGo), 1);
    const TimeRep share_ms     = std::max(TimeRep{0}, remaining_ms / moves_to_go);
    const TimeRep available_ms =
        increment_ms > MaxTimeMs - share_ms ? MaxTimeMs : share_ms + increment_ms;
    const TimeRep buffered_ms = available_ms > MoveOverheadMs ? available_ms - MoveOverheadMs : 0;
    base_target = Milliseconds{std::min(clock_cap_ms, std::max(MinimumTargetMs, buffered_ms))};
    hard_budget = base_target;

    // A depth below the engine ceiling, node limit, or mate limit keeps a fixed
    // clock budget. Only ordinary clock play enables savings and extensions.
    if (limits.depth != Limits::max_depth || limits.nodes || limits.mate)
        return;

    hard_budget = Milliseconds{scale_capped(base_target.count(), HardLimitPercent, clock_cap_ms)};
    soft_target = base_target;
}

void TimeManager::update_after_iteration(int depth, Move best_move, EvalValue score) {
    if (!soft_target)
        return;

    assert(depth >= 1 && !best_move.is_null());
    assert(score > -eval_value::inf && score < eval_value::inf);

    const bool has_previous_move = !previous_best_move.is_null();
    const bool same_best_move    = has_previous_move && best_move == previous_best_move;
    stable_move_iterations =
        same_best_move ? std::min(stable_move_iterations + 1, MaxStableMoveIterations) : 0;

    if (depth >= FeedbackStartDepth) {
        int percent = 100;
        if (same_best_move)
            percent -= StabilityDiscountPercent * stable_move_iterations;
        else if (has_previous_move)
            percent = ChangedMovePercent;

        // Mate distance is not a centipawn swing; only compare ordinary scores.
        if (has_previous_move && std::abs(previous_score) <= eval_value::mate_bound
            && std::abs(score) <= eval_value::mate_bound)
            percent += std::clamp(
                (previous_score - score) / ScoreDropCpPerPercent, 0, MaxScoreDropPercent);
        soft_target =
            Milliseconds{scale_capped(base_target.count(), percent, hard_budget->count())};
    }

    previous_best_move = best_move;
    previous_score     = score;
}

} // namespace search
