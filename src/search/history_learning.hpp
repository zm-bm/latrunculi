#pragma once

#include <algorithm>
#include <array>

#include "board/board.hpp"
#include "core/move.hpp"
#include "search/history.hpp"
#include "search/instrumentation.hpp"
#include "tuning/parameters.hpp"

namespace search {

// Per-node learning policy and feedback, borrowing the worker's history. Restore the board
// before reporting an outcome; pruned and stopped moves must not train history.
class HistoryLearning {
public:
    HistoryLearning(const Board& board,
                    History& history,
                    const OrderingContext& context,
                    int depth,
                    Instrumentation<>& stats,
                    const tuning::Parameters& parameters = tuning::defaults)
        : board(board),
          history(history),
          context(context),
          depth(depth),
          allow_quiet_penalties(depth >= parameters.QuietPenaltyMinDepth && !context.in_check),
          stats(stats),
          parameters(parameters) {
        if (allow_quiet_penalties)
            stats.quiet_penalty_eligible_node(depth);
    }

    // Borrow the same named context as the move stream for this node.
    HistoryLearning(const Board&,
                    History&,
                    const OrderingContext&&,
                    int,
                    Instrumentation<>&,
                    const tuning::Parameters& = tuning::defaults) = delete;

    HistoryLearning(const HistoryLearning&) = delete;
    HistoryLearning(HistoryLearning&&) = delete;
    HistoryLearning& operator=(const HistoryLearning&) = delete;
    HistoryLearning& operator=(HistoryLearning&&) = delete;

    // Buffer eligible moves, including alpha improvements that did not reach beta.
    // Classification predates the child search, which may have changed live killer entries.
    void record_non_cutoff(Move move, bool is_capture, bool was_killer) {
        if (move.type() == MOVE_PROM)
            return;

        if (is_capture) {
            if (!context.in_check && capture_count != NonCutoffCapacity)
                non_cutoff_captures[capture_count++] = move;
        } else if (allow_quiet_penalties
                   && move != context.tt_move
                   && !was_killer
                   && quiet_count != NonCutoffCapacity) {
            non_cutoff_quiets[quiet_count++] = move;
            stats.quiet_penalty_candidate(depth);
        }
    }

    // Train immediately: reward the cutoff and penalize eligible recorded alternatives.
    [[gnu::always_inline]]
    void learn_from_cutoff(Move move, bool is_capture) {
        const bool is_promotion = move.type() == MOVE_PROM;
        if (!is_capture && !is_promotion) {
            stats.quiet_cutoff(depth);
            history.update_quiet_refutations(context, move);
            history.update_quiet(context, board, move, quiet_bonus(depth));
            if (allow_quiet_penalties && quiet_count >= parameters.QuietPenaltyMinMoves) {
                const int penalty = -quiet_bonus(depth, parameters.QuietPenaltyDivisor);
                for (int i = 0; i < quiet_count; ++i) {
                    history.update_quiet(context, board, non_cutoff_quiets[i], penalty);
                    stats.quiet_penalty_update(depth);
                }
            }
        }
        const bool reward_capture =
            is_capture && !is_promotion && depth >= parameters.CaptureHistoryMinRewardDepth;
        if (!context.in_check && (reward_capture || capture_count != 0)) {
            const int bonus = capture_bonus(depth);
            if (reward_capture)
                history.update_capture(board, move, bonus);
            for (int i = 0; i < capture_count; ++i)
                history.update_capture(board, non_cutoff_captures[i], -bonus);
        }
    }

private:
    static constexpr int NonCutoffCapacity = 32;

    static int quiet_bonus(int depth, int divisor = 1) {
        return std::clamp(depth * depth / divisor, 0, History::QuietMaxScore);
    }

    int capture_bonus(int depth) const {
        return parameters.CaptureLearningRate
            * std::clamp(depth * depth, 0, parameters.CaptureDepthSquaredLimit);
    }

    const Board& board;
    History& history;
    const OrderingContext& context;
    const int depth;
    const bool allow_quiet_penalties;
    Instrumentation<>& stats;
    const tuning::Parameters& parameters;
    // Only recorded entries are initialized; both counts stay within the shared bound.
    std::array<Move, NonCutoffCapacity> non_cutoff_quiets;
    int quiet_count{0};
    std::array<Move, NonCutoffCapacity> non_cutoff_captures;
    int capture_count{0};
};

} // namespace search
