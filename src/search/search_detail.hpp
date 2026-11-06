#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>

#include "board/board.hpp"
#include "eval/parameters.hpp"
#include "search/node_type.hpp"
#include "search/search_frame.hpp"
#include "search/tt.hpp"

namespace search::detail {

inline void prefetch_child_tt(PositionKey tt_key) noexcept {
    tt.prefetch(tt_key);
}

// Null-search roots suppress early pruning and singular verification, but retain
// ordinary TT and move-loop behavior. A loaded position without history is not a null edge.
inline bool after_null_move(const Board& board, int ply) noexcept {
    return ply > 0 && board.previous_move().is_null() && board.can_unmake();
}

struct SingularVerification {
    EvalValue beta;
    int       depth;
    bool      multicut_allowed;
};

template <NodeType Node>
std::optional<SingularVerification> singular_verification(const Board&       board,
                                                          const TTRecord&    record,
                                                          const SearchFrame& frame,
                                                          int                ply,
                                                          int                root_depth,
                                                          int                depth,
                                                          EvalValue          alpha,
                                                          EvalValue          beta,
                                                          bool               verification_active);

// Exclusion belongs to this ply; verification stays active throughout the call.
// Restore the entire frame before interpreting the result, including after a stop.
class ScopedSingularVerification {
public:
    ScopedSingularVerification(SearchFrame& frame, bool& active, Move excluded_move) noexcept
        : frame(frame),
          saved(frame),
          active(active),
          was_active(active) {
        assert(!excluded_move.is_null());
        frame.excluded_move = excluded_move;
        active              = true;
    }
    ~ScopedSingularVerification() {
        frame  = saved;
        active = was_active;
    }

    ScopedSingularVerification(const ScopedSingularVerification&)            = delete;
    ScopedSingularVerification& operator=(const ScopedSingularVerification&) = delete;

private:
    SearchFrame&      frame;
    const SearchFrame saved;
    bool&             active;
    const bool        was_active;
};

class AspirationWindow {
public:
    enum class Outcome { Accepted, FailLow, FailHigh };

    AspirationWindow(int depth, EvalValue previous_value) noexcept
        : alpha(std::max(previous_value - InitialDelta, -eval_value::inf)),
          beta(std::min(previous_value + InitialDelta, eval_value::inf)),
          retry_depth(depth),
          nominal_depth(depth) {
        assert(depth >= 1);
    }

    // Inputs for the next root attempt; only this transition changes them.
    EvalValue alpha;
    EvalValue beta;
    int       retry_depth;

    Outcome accept_or_widen(EvalValue value) noexcept {
        Outcome outcome;
        if (value <= alpha) {
            alpha       = std::max(value - delta, -eval_value::inf);
            retry_depth = nominal_depth;
            outcome     = Outcome::FailLow;
        } else if (value >= beta) {
            beta        = std::min(value + delta, eval_value::inf);
            retry_depth = value <= -eval_value::mate_bound || value >= eval_value::mate_bound
                            ? nominal_depth
                            : std::max(1, nominal_depth - 1);
            outcome     = Outcome::FailHigh;
        } else {
            return Outcome::Accepted;
        }

        // Recenter with the current delta, then grow it for the next miss.
        const EvalValue growth = delta / 2;
        delta = delta >= eval_value::inf - growth ? eval_value::inf : delta + growth;
        return outcome;
    }

private:
    static constexpr EvalValue InitialDelta = 50;
    EvalValue                  delta{InitialDelta};
    const int                  nominal_depth;
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
