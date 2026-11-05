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

class SearchTestAccess;

namespace search {

// Immutable ordering inputs shared by move selection and learning. Hints are raw
// node-start snapshots; history scores remain live until their consumer reads them.
struct OrderingContext {
    static constexpr int           PreviousKeyCount = N_COLORS * piece_slots * N_SQUARES;
    static constexpr std::uint16_t NoPreviousKey    = PreviousKeyCount;

    Color         side{WHITE};
    bool          in_check{false};
    std::uint16_t previous_key{NoPreviousKey};
    int           ply{0};
    Move          tt_move{NULL_MOVE};
    Move          primary_killer{NULL_MOVE};
    Move          secondary_killer{NULL_MOVE};
    Move          counter_move{NULL_MOVE};

    bool                 has_previous_move() const { return previous_key != NoPreviousKey; }
    static std::uint16_t previous_index(Color side, PieceType piece, Square to) {
        return static_cast<std::uint16_t>((static_cast<int>(side) * piece_slots + piece_slot(piece))
                                              * N_SQUARES
                                          + static_cast<int>(to));
    }
};

// Persistent ordering memory owned by a worker. Refutations reset between searches;
// quiet/capture scores age, while continuation scores persist until full clearing.
class History {
public:
    static constexpr int QuietMaxScore   = 1024;
    static constexpr int CaptureMaxScore = 16384;

    History() : continuation_scores(std::make_unique<ContinuationTable>()) {}

    // Build once before constructing the node's stream and learning object.
    OrderingContext context_for(const Board& board, int ply, Move tt_move = NULL_MOVE) const {
        OrderingContext context{.side = board.side_to_move(), .ply = ply, .tt_move = tt_move};
        const Move      previous = board.previous_move();
        if (!previous.is_null()) {
            const PieceType piece =
                previous.type() == MOVE_PROM ? PAWN : type_of(board.piece_on(previous.to()));
            if (piece != NO_PIECETYPE)
                context.previous_key =
                    OrderingContext::previous_index(~context.side, piece, previous.to());
        }
        context.primary_killer   = killer_moves[ply][0];
        context.secondary_killer = killer_moves[ply][1];
        context.counter_move =
            context.has_previous_move() ? counter_moves[context.previous_key] : NULL_MOVE;
        context.in_check = board.is_check();
        return context;
    }

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

    void update_quiet_refutations(const OrderingContext& context, Move move) {
        const int ply = context.ply;
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

    int quiet_score(const OrderingContext& context, const Board& board, Move move) const {
        if (!context.has_previous_move())
            return base_quiet_score(context.side, move);
        return quiet_score(context, board.piece_type_on(move.from()), move);
    }

    // Generation already knows the original mover; no board lookup is needed.
    int quiet_score(const OrderingContext& context, PieceType piece, Move move) const {
        int score = base_quiet_score(context.side, move);
        if (context.has_previous_move() && piece != NO_PIECETYPE)
            score += continuation_entry(context, piece, move.to());
        return score;
    }

    void
    update_quiet(const OrderingContext& context, const Board& board, Move move, int adjustment) {
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
        return capture_history_score(board.side_to_move(),
                                     board.piece_type_on(move.from()),
                                     move.to(),
                                     board.captured_piece_type(move));
    }

    int capture_history_score(Color side, PieceType piece, Square to, PieceType victim) const {
        return capture_scores[side][piece_slot(piece)][to][piece_slot(victim)];
    }

    void update_capture(const Board& board, Move move, int adjustment) {
        assert(board.is_capture(move) && move.type() != MOVE_PROM);
        apply_gravity(capture_entry(board, move), adjustment, CaptureMaxScore);
    }

private:
    using Score                       = std::int16_t;
    static constexpr int MoveKeyCount = piece_slots * N_SQUARES;
    using ContinuationTable = std::array<Score, OrderingContext::PreviousKeyCount * MoveKeyCount>;

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

    static int continuation_index(std::uint16_t previous_key, PieceType piece, Square to) {
        const int move_key = piece_slot(piece) * N_SQUARES + static_cast<int>(to);
        return previous_key * MoveKeyCount + move_key;
    }

    Score& continuation_entry(const OrderingContext& context, PieceType piece, Square to) {
        return (*continuation_scores)[continuation_index(context.previous_key, piece, to)];
    }

    const Score&
    continuation_entry(const OrderingContext& context, PieceType piece, Square to) const {
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
    Move  counter_moves[OrderingContext::PreviousKeyCount]{};
    Score quiet_scores[N_COLORS][N_SQUARES][N_SQUARES]{};
    Score capture_scores[N_COLORS][piece_slots][N_SQUARES][piece_slots]{};
    // Keep the large continuation table off the worker's inline storage.
    std::unique_ptr<ContinuationTable> continuation_scores;

    friend class ::SearchTestAccess;
};

} // namespace search
