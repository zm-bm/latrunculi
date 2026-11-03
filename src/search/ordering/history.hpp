#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>

#include "board/board.hpp"
#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "search/instrumentation.hpp"

class SearchTestAccess;

namespace search::ordering {

// Keys for one node, shared by the picker and learning policy; contains no scores.
struct NodeContext {
    static constexpr int           PreviousKeyCount = N_COLORS * piece_slots * N_SQUARES;
    static constexpr std::uint16_t NoPreviousKey    = PreviousKeyCount;

    Color         side{WHITE};
    std::uint16_t previous_key{NoPreviousKey};

    bool                 has_previous_move() const { return previous_key != NoPreviousKey; }
    static std::uint16_t previous_index(Color side, PieceType piece, Square to) {
        return static_cast<std::uint16_t>((static_cast<int>(side) * piece_slots + piece_slot(piece))
                                              * N_SQUARES
                                          + static_cast<int>(to));
    }

    static NodeContext from_board(const Board& board) {
        NodeContext context{.side = board.side_to_move()};

        const Move prev_move = board.previous_move();
        if (prev_move.is_null())
            return context;

        const PieceType prev_piece =
            prev_move.type() == MOVE_PROM ? PAWN : type_of(board.piece_on(prev_move.to()));
        if (prev_piece == NO_PIECETYPE)
            return context;

        context.previous_key = previous_index(~context.side, prev_piece, prev_move.to());
        return context;
    }
};

// Persistent ordering memory owned by a worker. Refutations reset between searches;
// quiet/capture scores age, while continuation scores persist until full clearing.
class History {
public:
    static constexpr int QuietMaxScore   = 1024;
    static constexpr int CaptureMaxScore = 16384;

    History() : continuation_scores(std::make_unique<ContinuationTable>()) {}

    // Clear refutations and age quiet/capture scores; continuation scores persist.
    void prepare_for_search() {
        clear_refutations();
        for (auto& side : quiet_scores)
            for (auto& from : side)
                for (auto& entry : from)
                    age_entry(entry);
        for (auto& side : capture_scores)
            for (auto& piece : side)
                for (auto& to : piece)
                    for (auto& victim : to)
                        age_entry(victim);
    }

    // Reset all ordering memory, reusing the continuation allocation.
    void clear() {
        clear_refutations();
        for (auto& side : quiet_scores)
            for (auto& from : side)
                for (auto& entry : from)
                    entry = 0;
        for (auto& side : capture_scores)
            for (auto& piece : side)
                for (auto& to : piece)
                    for (auto& victim : to)
                        victim = 0;
        std::fill(continuation_scores->begin(), continuation_scores->end(), Score{0});
    }

    bool is_killer(Move move, int ply) const {
        return move == killer_moves[ply][0] || move == killer_moves[ply][1];
    }

    Move primary_killer(int ply) const { return killer_moves[ply][0]; }

    Move secondary_killer(int ply) const { return killer_moves[ply][1]; }

    Move counter_hint(const NodeContext& context) const {
        return context.has_previous_move() ? counter_moves[context.previous_key] : NULL_MOVE;
    }

    void update_quiet_refutations(const NodeContext& context, Move move, int ply) {
        if (killer_moves[ply][0] != move) {
            killer_moves[ply][1] = killer_moves[ply][0];
            killer_moves[ply][0] = move;
        }
        if (context.has_previous_move())
            counter_moves[context.previous_key] = move;
    }

    int base_quiet_score(Color side, Move move) const {
        return quiet_scores[side][move.from()][move.to()];
    }

    int quiet_score(const NodeContext& context, const Board& board, Move move) const {
        int score = base_quiet_score(context.side, move);
        if (!context.has_previous_move())
            return score;

        const PieceType piece = board.piece_type_on(move.from());
        if (piece != NO_PIECETYPE)
            score += continuation_entry(context, piece, move.to());
        return score;
    }

    void update_quiet(const NodeContext& context, const Board& board, Move move, int adjustment) {
        apply_gravity(quiet_scores[context.side][move.from()][move.to()], adjustment);
        if (!context.has_previous_move())
            return;

        const PieceType piece = board.piece_type_on(move.from());
        if (piece != NO_PIECETYPE)
            apply_gravity(continuation_entry(context, piece, move.to()), adjustment);
    }

    // Raw learned history; capture ordering applies its own scaling and material weights.
    int capture_history_score(const Board& board, Move move) const {
        assert(board.is_capture(move) && move.type() != MOVE_PROM);
        return capture_entry(board, move);
    }

    void update_capture(const Board& board, Move move, int adjustment) {
        assert(board.is_capture(move) && move.type() != MOVE_PROM);
        apply_gravity(capture_entry(board, move), adjustment, CaptureMaxScore);
    }

private:
    using Score                       = std::int16_t;
    static constexpr int MoveKeyCount = piece_slots * N_SQUARES;
    using ContinuationTable = std::array<Score, NodeContext::PreviousKeyCount * MoveKeyCount>;

    void clear_refutations() {
        for (auto& ply : killer_moves) {
            ply[0] = NULL_MOVE;
            ply[1] = NULL_MOVE;
        }
        for (auto& entry : counter_moves)
            entry = NULL_MOVE;
    }

    Score& capture_entry(const Board& board, Move move) {
        return capture_scores[board.side_to_move()][piece_slot(board.piece_type_on(move.from()))]
                             [move.to()][piece_slot(board.captured_piece_type(move))];
    }

    const Score& capture_entry(const Board& board, Move move) const {
        return capture_scores[board.side_to_move()][piece_slot(board.piece_type_on(move.from()))]
                             [move.to()][piece_slot(board.captured_piece_type(move))];
    }

    static int continuation_index(std::uint16_t previous_key, PieceType piece, Square to) {
        const int move_key = piece_slot(piece) * N_SQUARES + static_cast<int>(to);
        return previous_key * MoveKeyCount + move_key;
    }

    Score& continuation_entry(const NodeContext& context, PieceType piece, Square to) {
        return (*continuation_scores)[continuation_index(context.previous_key, piece, to)];
    }

    const Score& continuation_entry(const NodeContext& context, PieceType piece, Square to) const {
        return (*continuation_scores)[continuation_index(context.previous_key, piece, to)];
    }

    // Apply bounded gravity so entries approach saturation smoothly.
    static void apply_gravity(Score& entry, int adjustment, int limit = QuietMaxScore) {
        adjustment        = std::clamp(adjustment, -limit, limit);
        const int current = entry;
        const int weight  = adjustment < 0 ? -adjustment : adjustment;
        const int gravity = adjustment - (current * weight / limit);

        entry = std::clamp(current + gravity, -limit, limit);
    }

    static void age_entry(Score& entry) { entry /= 2; }

    Move  killer_moves[engine::max_search_ply][2]{};
    Move  counter_moves[NodeContext::PreviousKeyCount]{};
    Score quiet_scores[N_COLORS][N_SQUARES][N_SQUARES]{};
    Score capture_scores[N_COLORS][piece_slots][N_SQUARES][piece_slots]{};
    // Keep the large continuation table off the worker's inline storage.
    std::unique_ptr<ContinuationTable> continuation_scores;

    friend class ::SearchTestAccess;
};

// Per-node learning policy and feedback, borrowing the worker's history. Restore the board
// before reporting an outcome; pruned and stopped moves must not train history.
class NodeLearning {
public:
    NodeLearning(const Board&       board,
                 History&           history,
                 NodeContext        context,
                 int                ply,
                 int                depth,
                 Move               tt_move,
                 Instrumentation<>& stats)
        : board(board),
          history(history),
          context(context),
          ply(ply),
          depth(depth),
          tt_move(tt_move),
          in_check(board.is_check()),
          allow_quiet_malus(depth >= QuietMalusMinDepth && !in_check),
          stats(stats) {
        if (allow_quiet_malus)
            stats.quiet_malus_eligible_node(depth);
    }

    // Includes alpha improvements that did not reach beta.
    void record_non_cutoff(Move move, bool is_capture, bool is_killer) {
        if (move.type() == MOVE_PROM)
            return;

        if (is_capture) {
            if (!in_check && capture_count < FailedCapacity)
                failed_captures[capture_count++] = move;
        } else if (allow_quiet_malus && move != tt_move && !is_killer
                   && quiet_count < FailedCapacity) {
            failed_quiets[quiet_count++] = move;
            stats.quiet_malus_failed_quiet(depth);
        }
    }

    [[gnu::always_inline]] void record_cutoff(Move move, bool is_capture) {
        const bool is_promotion = move.type() == MOVE_PROM;
        if (!is_capture && !is_promotion) {
            stats.quiet_cutoff(depth);
            history.update_quiet_refutations(context, move, ply);
            history.update_quiet(context, board, move, quiet_bonus(depth));
            if (allow_quiet_malus && quiet_count >= QuietMalusMinFailed) {
                const int penalty = -quiet_bonus(depth, QuietMalusDivisor);
                for (int i = 0; i < quiet_count; ++i) {
                    history.update_quiet(context, board, failed_quiets[i], penalty);
                    stats.quiet_malus_update(depth);
                }
            }
        }
        const bool reward_capture =
            is_capture && !is_promotion && depth >= CaptureHistoryMinRewardDepth;
        if (!in_check && (reward_capture || capture_count != 0)) {
            const int bonus = capture_bonus(depth);
            if (reward_capture)
                history.update_capture(board, move, bonus);
            for (int i = 0; i < capture_count; ++i)
                history.update_capture(board, failed_captures[i], -bonus);
        }
    }

private:
    static constexpr int QuietMalusMinDepth           = 4;
    static constexpr int QuietMalusMinFailed          = 2;
    static constexpr int QuietMalusDivisor            = 2;
    static constexpr int CaptureHistoryMinRewardDepth = 3;
    static constexpr int FailedCapacity               = 32;
    static constexpr int CaptureLearningRate          = 16;
    static constexpr int CaptureMaxDepthBonus         = 128;

    static int quiet_bonus(int depth, int divisor = 1) {
        return std::clamp(depth * depth / std::max(1, divisor), 0, History::QuietMaxScore);
    }

    static int capture_bonus(int depth) {
        return CaptureLearningRate * std::clamp(depth * depth, 0, CaptureMaxDepthBonus);
    }

    const Board&                     board;
    History&                         history;
    const NodeContext                context;
    const int                        ply;
    const int                        depth;
    const Move                       tt_move;
    const bool                       in_check;
    const bool                       allow_quiet_malus;
    Instrumentation<>&               stats;
    std::array<Move, FailedCapacity> failed_quiets;
    std::array<Move, FailedCapacity> failed_captures;
    int                              quiet_count{0};
    int                              capture_count{0};
};

} // namespace search::ordering
