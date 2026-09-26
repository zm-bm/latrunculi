#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "eval/parameters.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"

namespace search::algorithm_detail {

// Aspiration-window defaults.
inline constexpr EvalValue AspirationWindow = 50;

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

inline constexpr std::array<int, 20> HelperDepthSkipSize{
    1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
};
inline constexpr std::array<int, 20> HelperDepthSkipPhase{
    0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7,
};

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

// Late-move reduction defaults.
inline constexpr int LmrMinDepth     = 3;
inline constexpr int LmrMinMoveCount = 4;

// Late-move pruning defaults.
inline constexpr int LateMovePruningDepth     = 2;
inline constexpr int LateMovePruningMoveCount = 12;

// Quiet-history malus defaults.
inline constexpr int QuietMalusMinDepth  = 4;
inline constexpr int QuietMalusMinFailed = 2;
inline constexpr int QuietMalusDivisor   = 2;

// Apply the PV/non-PV TT cutoff policy.
template <NodeType Node>
bool tt_cutoff_allowed(
    const TTRecord& record, EvalValue adjusted_score, int depth, EvalValue alpha, EvalValue beta) {
    if constexpr (Node == NodeType::Pv)
        return int(record.depth) >= depth && record.bound == TTBound::Exact;

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

// Late-move reduction formula.
template <NodeType Node>
int lmr_reduction(int  depth,
                  int  move_count,
                  bool is_quiet,
                  bool is_promotion,
                  bool in_check,
                  bool gives_check,
                  bool is_killer) {
    if (depth < LmrMinDepth || move_count < LmrMinMoveCount)
        return 0;

    // Do not reduce moves that create immediate tactical obligations.
    if (is_promotion || in_check || gives_check)
        return 0;

    // Use the LMR formula as a starting point.
    const double base = is_quiet ? 1.25 : 0.75;
    const double div  = is_quiet ? 2.5 : 3.3;
    double       r    = base + std::log(depth) * std::log(move_count) / div;

    // Reduce less for PV and killer moves.
    if constexpr (Node == NodeType::Pv)
        r *= 0.7;
    if (is_killer)
        r *= 0.8;

    // Do not extend or drop straight into qsearch.
    return std::clamp(static_cast<int>(r), 1, depth - 2);
}

struct FailedQuiets {
    static constexpr int Capacity = 32;

    bool add(Move move) {
        if (count_ >= Capacity)
            return false;

        moves_[count_++] = move;
        return true;
    }

    int size() const { return count_; }

    template <typename Fn>
    void for_each(Fn fn) const {
        for (int i = 0; i < count_; ++i)
            fn(moves_[i]);
    }

private:
    Move moves_[Capacity];
    int  count_{0};
};

} // namespace search::algorithm_detail
