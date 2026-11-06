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

// Transposition-table cutoffs.
// A cached score has no continuation, so PV nodes must search.
template <NodeType Node>
bool tt_cutoff_allowed(
    const TTRecord& record, EvalValue adjusted_score, int depth, EvalValue alpha, EvalValue beta) {
    if constexpr (Node == NodeType::Pv)
        return false;

    return record.can_cutoff(adjusted_score, depth, alpha, beta);
}

// Early and quiet-move pruning.
// Null-search roots suppress early pruning and singular verification, but retain
// ordinary TT and move-loop behavior. A loaded position without history is not a null edge.
inline bool after_null_move(const Board& board, int ply) noexcept {
    return ply > 0 && board.previous_move().is_null() && board.can_unmake();
}

// Null-move pruning defaults.
inline constexpr int       NullMoveMinDepth        = 3;
inline constexpr int       NullMoveReductionBase   = 3;
inline constexpr int       NullMoveReductionMax    = 6;
inline constexpr int       NullMoveDepthDivisor    = 7;
inline constexpr EvalValue NullMoveSurplusPerPly   = 2 * eval::pawn.mg;
inline constexpr int       NullMoveSurplusBonusMax = 2;

inline int
null_move_reduction(const int depth, const EvalValue static_eval, const EvalValue beta) noexcept {
    assert(depth >= NullMoveMinDepth);

    const EvalValue positive_surplus = std::max(static_eval - beta, EvalValue{0});
    const int       surplus_bonus =
        std::min(int(positive_surplus / NullMoveSurplusPerPly), NullMoveSurplusBonusMax);
    const int reduction = NullMoveReductionBase + depth / NullMoveDepthDivisor + surplus_bonus;

    return std::min({depth, NullMoveReductionMax, reduction});
}

// Razoring and futility defaults.
inline constexpr int RazorMaxDepth    = 3;
inline constexpr int FutilityMaxDepth = 3;
inline constexpr int RazorMargin[]    = {0, 500, 900, 1800};
inline constexpr int FutilityMargin[] = {0, 250, 400, 550};

// Late-move pruning defaults.
inline constexpr int LateMovePruningDepth     = 2;
inline constexpr int LateMovePruningMoveCount = 12;

// Singular verification policy and same-ply restoration.
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

// Late-move reduction defaults.
inline constexpr int LmrMinDepth              = 3;
inline constexpr int LmrMinMoveCount          = 4;
inline constexpr int CaptureLmrMinDepth       = 4;
inline constexpr int CaptureLmrHistoryDivisor = 8192;

struct LmrInputs {
    int  depth;
    int  move_count;
    bool is_quiet{false};
    bool is_promotion{false};
    bool in_check{false};
    bool gives_check{false};
    bool is_killer{false};
    bool declining{false};
    bool is_tt_move{false};
    int  capture_history{0};
};

struct LmrReduction {
    int  plies{0};
    int  base_plies{0};
    bool capture_adjustment{false};
};

// Complete reduction policy. Capture history adjusts the fractional base before
// rounding; retain the integer base and eligibility for search instrumentation.
template <NodeType Node>
LmrReduction lmr_reduction(const LmrInputs& input) {
    if (input.depth < LmrMinDepth || input.move_count < LmrMinMoveCount)
        return {};

    // Do not reduce moves that create immediate tactical obligations.
    if (input.is_promotion || input.in_check || input.gives_check)
        return {};

    const double offset  = input.is_quiet ? 1.25 : 0.75;
    const double divisor = input.is_quiet ? 2.5 : 3.3;
    double       fractional_reduction =
        offset + std::log(input.depth) * std::log(input.move_count) / divisor;

    // Reduce less for PV and killer moves.
    if constexpr (Node == NodeType::Pv)
        fractional_reduction *= 0.7;
    if (input.is_killer)
        fractional_reduction *= 0.8;

    // Keep at least one main-search ply after the move.
    int reduction = std::clamp(static_cast<int>(fractional_reduction), 1, input.depth - 2);
    if constexpr (Node == NodeType::NonPv) {
        if (input.declining && input.is_quiet && !input.is_killer && reduction < input.depth - 2)
            ++reduction;
    }
    LmrReduction result{.plies = reduction, .base_plies = reduction};
    if constexpr (Node == NodeType::NonPv) {
        // Promotions and checking moves exited above; a non-quiet move is a capture.
        if (!input.is_quiet && !input.is_tt_move && input.depth >= CaptureLmrMinDepth) {
            const double adjustment = std::clamp(
                -static_cast<double>(input.capture_history) / CaptureLmrHistoryDivisor, -1.0, 1.0);
            result.plies =
                std::clamp(static_cast<int>(fractional_reduction + adjustment), 0, input.depth - 2);
            result.capture_adjustment = true;
        }
    }
    return result;
}

// Positional allowance for the heuristic qsearch capture-gain estimate.
inline constexpr EvalValue QCaptureMargin = 300;

} // namespace search::detail
