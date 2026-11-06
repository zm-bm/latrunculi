#pragma once

#include <algorithm>
#include <array>

#include "board/board.hpp"
#include "core/move.hpp"
#include "search/history.hpp"
#include "search/instrumentation.hpp"

namespace search {

// Per-node learning policy and feedback, borrowing the worker's history. Restore the board
// before reporting an outcome; pruned and stopped moves must not train history.
class HistoryLearning {
public:
    HistoryLearning(const Board&           board,
                    History&               history,
                    const OrderingContext& context,
                    int                    depth,
                    Instrumentation<>&     stats)
        : board(board),
          history(history),
          context(context),
          depth(depth),
          allow_quiet_malus(depth >= QuietMalusMinDepth && !context.in_check),
          stats(stats) {
        if (allow_quiet_malus)
            stats.quiet_malus_eligible_node(depth);
    }

    // Borrow the same named context as the move stream for this node.
    HistoryLearning(const Board&, History&, const OrderingContext&&, int, Instrumentation<>&) =
        delete;

    HistoryLearning(const HistoryLearning&)            = delete;
    HistoryLearning(HistoryLearning&&)                 = delete;
    HistoryLearning& operator=(const HistoryLearning&) = delete;
    HistoryLearning& operator=(HistoryLearning&&)      = delete;

    // Includes alpha improvements that did not reach beta. Classification is from
    // before the child search, which may have changed the live killer entries.
    void record_non_cutoff(Move move, bool is_capture, bool was_killer) {
        if (move.type() == MOVE_PROM)
            return;

        if (is_capture) {
            if (!context.in_check && capture_count != NonCutoffCapacity)
                non_cutoff_captures[capture_count++] = move;
        } else if (allow_quiet_malus && move != context.tt_move && !was_killer
                   && quiet_count != NonCutoffCapacity) {
            non_cutoff_quiets[quiet_count++] = move;
            stats.quiet_malus_failed_quiet(depth);
        }
    }

    [[gnu::always_inline]] void record_cutoff(Move move, bool is_capture) {
        const bool is_promotion = move.type() == MOVE_PROM;
        if (!is_capture && !is_promotion) {
            stats.quiet_cutoff(depth);
            history.update_quiet_refutations(context, move);
            history.update_quiet(context, board, move, quiet_bonus(depth));
            if (allow_quiet_malus && quiet_count >= QuietMalusMinNonCutoffs) {
                const int penalty = -quiet_bonus(depth, QuietMalusDivisor);
                for (int i = 0; i < quiet_count; ++i) {
                    history.update_quiet(context, board, non_cutoff_quiets[i], penalty);
                    stats.quiet_malus_update(depth);
                }
            }
        }
        const bool reward_capture =
            is_capture && !is_promotion && depth >= CaptureHistoryMinRewardDepth;
        if (!context.in_check && (reward_capture || capture_count != 0)) {
            const int bonus = capture_bonus(depth);
            if (reward_capture)
                history.update_capture(board, move, bonus);
            for (int i = 0; i < capture_count; ++i)
                history.update_capture(board, non_cutoff_captures[i], -bonus);
        }
    }

private:
    static constexpr int QuietMalusMinDepth           = 4;
    static constexpr int QuietMalusMinNonCutoffs      = 2;
    static constexpr int QuietMalusDivisor            = 2;
    static constexpr int CaptureHistoryMinRewardDepth = 3;
    static constexpr int NonCutoffCapacity            = 32;
    static constexpr int CaptureLearningRate          = 16;
    static constexpr int CaptureMaxDepthBonus         = 128;

    static int quiet_bonus(int depth, int divisor = 1) {
        return std::clamp(depth * depth / divisor, 0, History::QuietMaxScore);
    }

    static int capture_bonus(int depth) {
        return CaptureLearningRate * std::clamp(depth * depth, 0, CaptureMaxDepthBonus);
    }

    const Board&           board;
    History&               history;
    const OrderingContext& context;
    const int              depth;
    const bool             allow_quiet_malus;
    Instrumentation<>&     stats;
    // Only recorded entries are initialized; both counts stay within the shared bound.
    std::array<Move, NonCutoffCapacity> non_cutoff_quiets;
    int                                 quiet_count{0};
    std::array<Move, NonCutoffCapacity> non_cutoff_captures;
    int                                 capture_count{0};
};

} // namespace search
