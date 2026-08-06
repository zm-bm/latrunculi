#pragma once

#include "board/board.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "search/ordering/history.hpp"
#include "search/ordering/refutations.hpp"

namespace search::ordering {

/*
 * State owns per-worker ordering state. Scored histories persist across
 * searches within a game; search-local refutations do not.
 */
struct State {
    /*
     * Context caches node-local keys derived from the board, avoiding repeated
     * previous-move extraction in hot move-ordering paths.
     */
    struct Context {
        Color     side{WHITE};
        Color     previous_side{WHITE};
        PieceType previous_piece{NO_PIECETYPE};
        Square    previous_to{INVALID};
        bool      has_previous_move{false};
    };

    KillerMoves         killers;
    CounterMoves        counters;
    QuietHistory        quiets;
    ContinuationHistory continuations;

    static Context make_context(const Board& board);

    void prepare_for_search();
    void clear();
    bool is_killer(Move move, int ply) const;
    Move counter_hint(const Context& context) const;
    void update_quiet_refutations(const Context& context, Move move, int ply);
    int  quiet_score(const Context& context,
                     const Board&   board,
                     Move           move,
                     bool           include_continuation) const;
    void reward_quiet(const Context& context, const Board& board, Move move, int depth);
    void penalize_quiet(
        const Context& context, const Board& board, Move move, int depth, int divisor = 1);

private:
    static PieceType moving_piece(const Board& board, Move move);
};

inline void State::prepare_for_search() {
    killers.clear();
    counters.clear();
    quiets.age();
}

inline void State::clear() {
    killers.clear();
    counters.clear();
    quiets.clear();
    continuations.clear();
}

inline bool State::is_killer(Move move, int ply) const {
    return killers.is_killer(move, ply);
}

inline Move State::counter_hint(const Context& context) const {
    return context.has_previous_move
             ? counters.get(context.previous_side, context.previous_piece, context.previous_to)
             : NULL_MOVE;
}

inline void State::update_quiet_refutations(const Context& context, Move move, int ply) {
    killers.update(move, ply);

    if (context.has_previous_move)
        counters.update(context.previous_side, context.previous_piece, context.previous_to, move);
}

inline int State::quiet_score(const Context& context,
                              const Board&   board,
                              Move           move,
                              bool           include_continuation) const {
    const Square from  = move.from();
    const Square to    = move.to();
    int          score = quiets.get(context.side, from, to);

    if (!include_continuation || !context.has_previous_move)
        return score;

    const PieceType piece = moving_piece(board, move);
    if (piece != NO_PIECETYPE)
        score += continuations.get(
            context.previous_side, context.previous_piece, context.previous_to, piece, to);

    return score;
}

inline void State::reward_quiet(const Context& context, const Board& board, Move move, int depth) {
    const Square from = move.from();
    const Square to   = move.to();
    quiets.reward(context.side, from, to, depth);

    if (!context.has_previous_move)
        return;

    const PieceType piece = moving_piece(board, move);
    if (piece != NO_PIECETYPE)
        continuations.reward(
            context.previous_side, context.previous_piece, context.previous_to, piece, to, depth);
}

inline void State::penalize_quiet(
    const Context& context, const Board& board, Move move, int depth, int divisor) {
    const Square from = move.from();
    const Square to   = move.to();
    quiets.penalize(context.side, from, to, depth, divisor);

    if (!context.has_previous_move)
        return;

    const PieceType piece = moving_piece(board, move);
    if (piece != NO_PIECETYPE)
        continuations.penalize(context.previous_side,
                               context.previous_piece,
                               context.previous_to,
                               piece,
                               to,
                               depth,
                               divisor);
}

inline State::Context State::make_context(const Board& board) {
    Context context{.side = board.side_to_move()};

    const Move prev_move = board.previous_move();
    if (prev_move.is_null())
        return context;

    const PieceType prev_piece =
        prev_move.type() == MOVE_PROM ? PAWN : type_of(board.piece_on(prev_move.to()));
    if (prev_piece == NO_PIECETYPE)
        return context;

    context.previous_side     = ~context.side;
    context.previous_piece    = prev_piece;
    context.previous_to       = prev_move.to();
    context.has_previous_move = true;
    return context;
}

inline PieceType State::moving_piece(const Board& board, Move move) {
    return type_of(board.piece_on(move.from()));
}

} // namespace search::ordering
