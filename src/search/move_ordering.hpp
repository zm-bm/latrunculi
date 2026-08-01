#pragma once

#include "board/board.hpp"
#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "search/history.hpp"

/*
 * Killer moves are quiet refutations indexed by ply. The picker tries the two
 * most recent killers after good captures and before generated quiet moves.
 */
struct KillerMoves {
    void update(Move killer, int ply);
    bool is_killer(Move move, int ply) const;
    Move primary(int ply) const;
    Move secondary(int ply) const;
    void clear();

private:
    Move table[engine::max_search_ply][2] = {NULL_MOVE};
};

inline void KillerMoves::update(Move killer, int ply) {
    if (table[ply][0] == killer)
        return;
    table[ply][1] = table[ply][0];
    table[ply][0] = killer;
}

inline bool KillerMoves::is_killer(Move move, int ply) const {
    return move == table[ply][0] || move == table[ply][1];
}

inline Move KillerMoves::primary(int ply) const {
    return table[ply][0];
}

inline Move KillerMoves::secondary(int ply) const {
    return table[ply][1];
}

inline void KillerMoves::clear() {
    for (int ply = 0; ply < engine::max_search_ply; ++ply) {
        table[ply][0] = NULL_MOVE;
        table[ply][1] = NULL_MOVE;
    }
}

/*
 * Counter moves remember one quiet reply to the previous moved piece's
 * destination square. The picker treats this as a refutation hint after killers.
 */
struct CounterMoves {
    Move get(Color prev_c, PieceType prev_piece, Square prev_to) const;
    void update(Color prev_c, PieceType prev_piece, Square prev_to, Move counter);
    void clear();

private:
    Move table[N_COLORS][piece_slots][N_SQUARES] = {NULL_MOVE};
};

inline Move CounterMoves::get(Color prev_c, PieceType prev_piece, Square prev_to) const {
    const int prev_slot = piece_slot(prev_piece);
    return table[prev_c][prev_slot][prev_to];
}

inline void CounterMoves::update(Color prev_c, PieceType prev_piece, Square prev_to, Move counter) {
    const int prev_slot = piece_slot(prev_piece);

    table[prev_c][prev_slot][prev_to] = counter;
}

inline void CounterMoves::clear() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int piece = 0; piece < piece_slots; ++piece) {
            for (int to = 0; to < N_SQUARES; ++to) {
                table[c][piece][to] = NULL_MOVE;
            }
        }
    }
}

/*
 * MoveOrdering owns per-worker ordering state. Scored histories persist across
 * searches within a game; search-local refutations do not.
 */
struct MoveOrdering {
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

inline void MoveOrdering::prepare_for_search() {
    killers.clear();
    counters.clear();
    quiets.age();
}

inline void MoveOrdering::clear() {
    killers.clear();
    counters.clear();
    quiets.clear();
    continuations.clear();
}

inline bool MoveOrdering::is_killer(Move move, int ply) const {
    return killers.is_killer(move, ply);
}

inline Move MoveOrdering::counter_hint(const Context& context) const {
    return context.has_previous_move
             ? counters.get(context.previous_side, context.previous_piece, context.previous_to)
             : NULL_MOVE;
}

inline void MoveOrdering::update_quiet_refutations(const Context& context, Move move, int ply) {
    killers.update(move, ply);

    if (context.has_previous_move)
        counters.update(context.previous_side, context.previous_piece, context.previous_to, move);
}

inline int MoveOrdering::quiet_score(const Context& context,
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

inline void
MoveOrdering::reward_quiet(const Context& context, const Board& board, Move move, int depth) {
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

inline void MoveOrdering::penalize_quiet(
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

inline MoveOrdering::Context MoveOrdering::make_context(const Board& board) {
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

inline PieceType MoveOrdering::moving_piece(const Board& board, Move move) {
    return type_of(board.piece_on(move.from()));
}
