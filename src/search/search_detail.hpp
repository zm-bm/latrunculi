#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>

#include "eval/parameters.hpp"
#include "search/node_type.hpp"
#include "search/tt.hpp"

namespace search::detail {

inline void prefetch_child_tt(PositionKey tt_key) noexcept {
    tt.prefetch(tt_key);
}

enum class AspirationMiss { FailLow, FailHigh };

inline int aspiration_retry_depth(const int            nominal_depth,
                                  const EvalValue      value,
                                  const AspirationMiss miss) noexcept {
    assert(nominal_depth >= 1);

    if (miss == AspirationMiss::FailLow || value <= -eval_value::mate_bound
        || value >= eval_value::mate_bound)
        return nominal_depth;

    return std::max(1, nominal_depth - 1);
}

inline EvalValue widen_aspiration_delta(const EvalValue delta) noexcept {
    assert(delta > 0 && delta <= eval_value::inf);

    const EvalValue growth = delta / 2;
    return delta >= eval_value::inf - growth ? eval_value::inf : delta + growth;
}

// Null-move pruning defaults.
inline constexpr int       NullMoveMinDepth        = 3;
inline constexpr int       NullMoveReductionBase   = 3;
inline constexpr int       NullMoveReductionMax    = 6;
inline constexpr int       NullMoveDepthDivisor    = 7;
inline constexpr EvalValue NullMoveSurplusPerPly   = 2 * eval::pawn.mg;
inline constexpr int       NullMoveSurplusBonusMax = 2;

// Razoring and futility defaults.
inline constexpr int RazorMaxDepth    = 3;
inline constexpr int FutilityMaxDepth = 3;
inline constexpr int RazorMargin[]    = {0, 500, 900, 1800};
inline constexpr int FutilityMargin[] = {0, 250, 400, 550};

// Positional allowance for the heuristic qsearch capture-gain estimate.
inline constexpr EvalValue QCaptureMargin = 300;

// Late-move reduction defaults.
inline constexpr int LmrMinDepth              = 3;
inline constexpr int LmrMinMoveCount          = 4;
inline constexpr int CaptureLmrMinDepth       = 4;
inline constexpr int CaptureLmrHistoryDivisor = 8192;

// Late-move pruning defaults.
inline constexpr int LateMovePruningDepth     = 2;
inline constexpr int LateMovePruningMoveCount = 12;

// A cached score has no continuation, so PV nodes must search.
template <NodeType Node>
bool tt_cutoff_allowed(
    const TTRecord& record, EvalValue adjusted_score, int depth, EvalValue alpha, EvalValue beta) {
    if constexpr (Node == NodeType::Pv)
        return false;

    return record.can_cutoff(adjusted_score, depth, alpha, beta);
}

inline int
null_move_reduction(const int depth, const EvalValue static_eval, const EvalValue beta) noexcept {
    assert(depth >= NullMoveMinDepth);

    const EvalValue positive_surplus = std::max(static_eval - beta, EvalValue{0});
    const int       surplus_bonus =
        std::min(int(positive_surplus / NullMoveSurplusPerPly), NullMoveSurplusBonusMax);
    const int reduction = NullMoveReductionBase + depth / NullMoveDepthDivisor + surplus_bonus;

    return std::min({depth, NullMoveReductionMax, reduction});
}

struct LmrReduction {
    int  plies{0};
    int  base_plies{0};
    bool capture_adjustment{false};
};

// Complete reduction policy. Capture history adjusts the fractional base before
// rounding; retain the integer base and eligibility for search instrumentation.
template <NodeType Node>
LmrReduction lmr_reduction(int  depth,
                           int  move_count,
                           bool is_quiet,
                           bool is_promotion,
                           bool in_check,
                           bool gives_check,
                           bool is_killer,
                           bool declining,
                           bool is_tt_move,
                           int  capture_history) {
    if (depth < LmrMinDepth || move_count < LmrMinMoveCount)
        return {};

    // Do not reduce moves that create immediate tactical obligations.
    if (is_promotion || in_check || gives_check)
        return {};

    const double base = is_quiet ? 1.25 : 0.75;
    const double div  = is_quiet ? 2.5 : 3.3;
    double       r    = base + std::log(depth) * std::log(move_count) / div;

    // Reduce less for PV and killer moves.
    if constexpr (Node == NodeType::Pv)
        r *= 0.7;
    if (is_killer)
        r *= 0.8;

    // Keep at least one main-search ply after the move.
    int reduction = std::clamp(static_cast<int>(r), 1, depth - 2);
    if constexpr (Node == NodeType::NonPv) {
        if (declining && is_quiet && !is_killer && reduction < depth - 2)
            ++reduction;
    }
    LmrReduction result{.plies = reduction, .base_plies = reduction};
    if constexpr (Node == NodeType::NonPv) {
        // Promotions and checking moves exited above; a non-quiet move is a capture.
        if (!is_quiet && !is_tt_move && depth >= CaptureLmrMinDepth) {
            const double adjustment = std::clamp(
                -static_cast<double>(capture_history) / CaptureLmrHistoryDivisor, -1.0, 1.0);
            result.plies              = std::clamp(static_cast<int>(r + adjustment), 0, depth - 2);
            result.capture_adjustment = true;
        }
    }
    return result;
}

} // namespace search::detail
