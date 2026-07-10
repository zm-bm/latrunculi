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

namespace mo_detail {

using Score = int16_t;

inline constexpr int PIECE_SLOTS = 6;
inline constexpr int MAX_SCORE   = 1024;

static_assert(PAWN == 1);
static_assert(KING == 6);
static_assert(N_PIECES == 7);
static_assert(static_cast<int>(KING) - static_cast<int>(PAWN) + 1 == PIECE_SLOTS);

inline int slot(PieceType piece) {
    assert(piece >= PAWN && piece <= KING);
    return static_cast<int>(piece) - static_cast<int>(PAWN);
}

inline void apply_signed_gravity(Score& entry, int max_score, int bonus) {
    bonus             = std::clamp(bonus, -max_score, max_score);
    const int current = entry;
    const int weight  = bonus < 0 ? -bonus : bonus;
    const int gravity = bonus - (current * weight / max_score);

    entry = std::clamp(current + gravity, -max_score, max_score);
}

} // namespace mo_detail

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
    Move table[N_COLORS][mo_detail::PIECE_SLOTS][N_SQUARES] = {NULL_MOVE};
};

inline Move CounterMoves::get(Color prev_c, PieceType prev_piece, Square prev_to) const {
    const int prev_slot = mo_detail::slot(prev_piece);
    return table[prev_c][prev_slot][prev_to];
}

inline void CounterMoves::update(Color prev_c, PieceType prev_piece, Square prev_to, Move counter) {
    const int prev_slot = mo_detail::slot(prev_piece);

    table[prev_c][prev_slot][prev_to] = counter;
}

inline void CounterMoves::clear() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int piece = 0; piece < mo_detail::PIECE_SLOTS; ++piece) {
            for (int to = 0; to < N_SQUARES; ++to) {
                table[c][piece][to] = NULL_MOVE;
            }
        }
    }
}

/*
 * Quiet history scores quiet moves by color, from, and to. Search rewards quiet
 * cutoffs and applies bounded maluses to failed ordinary quiets.
 */
struct QuietHistory {
    static constexpr int MAX_SCORE = mo_detail::MAX_SCORE;

    int  get(Color c, Square from, Square to) const;
    void reward(Color c, Square from, Square to, int depth);
    void penalize(Color c, Square from, Square to, int depth, int divisor = 1);
    void age();
    void clear();

private:
    void update(Color c, Square from, Square to, int bonus);

    mo_detail::Score table[N_COLORS][N_SQUARES][N_SQUARES] = {0};
};

inline int QuietHistory::get(Color c, Square from, Square to) const {
    return table[c][from][to];
}

inline void QuietHistory::reward(Color c, Square from, Square to, int depth) {
    const int bonus = std::clamp(depth * depth, 0, MAX_SCORE);
    update(c, from, to, bonus);
}

inline void QuietHistory::penalize(Color c, Square from, Square to, int depth, int divisor) {
    const int safe_divisor = std::max(1, divisor);
    const int bonus        = std::clamp(depth * depth / safe_divisor, 0, MAX_SCORE);
    update(c, from, to, -bonus);
}

inline void QuietHistory::update(Color c, Square from, Square to, int bonus) {
    mo_detail::apply_signed_gravity(table[c][from][to], MAX_SCORE, bonus);
}

inline void QuietHistory::age() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int from = 0; from < N_SQUARES; ++from) {
            for (int to = 0; to < N_SQUARES; ++to) {
                table[c][from][to] /= 2;
            }
        }
    }
}

inline void QuietHistory::clear() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int from = 0; from < N_SQUARES; ++from) {
            for (int to = 0; to < N_SQUARES; ++to) {
                table[c][from][to] = 0;
            }
        }
    }
}

/*
 * Capture history scores a moving piece to a destination against the captured
 * piece type. It is table-only scaffolding until capture ordering is revisited.
 */
struct CaptureHistory {
    static constexpr int MAX_SCORE = mo_detail::MAX_SCORE;

    int  get(Color c, PieceType piece, Square to, PieceType captured) const;
    void reward(Color c, PieceType piece, Square to, PieceType captured, int depth);
    void
    penalize(Color c, PieceType piece, Square to, PieceType captured, int depth, int divisor = 1);
    void age();
    void clear();

private:
    void update(Color c, PieceType piece, Square to, PieceType captured, int bonus);

    mo_detail::Score table[N_COLORS][mo_detail::PIECE_SLOTS][N_SQUARES][mo_detail::PIECE_SLOTS] = {
        0};
};

inline int CaptureHistory::get(Color c, PieceType piece, Square to, PieceType captured) const {
    const int piece_slot    = mo_detail::slot(piece);
    const int captured_slot = mo_detail::slot(captured);
    return table[c][piece_slot][to][captured_slot];
}

inline void
CaptureHistory::reward(Color c, PieceType piece, Square to, PieceType captured, int depth) {
    const int bonus = std::clamp(depth * depth, 0, MAX_SCORE);
    update(c, piece, to, captured, bonus);
}

inline void CaptureHistory::penalize(
    Color c, PieceType piece, Square to, PieceType captured, int depth, int divisor) {
    const int safe_divisor = std::max(1, divisor);
    const int bonus        = std::clamp(depth * depth / safe_divisor, 0, MAX_SCORE);
    update(c, piece, to, captured, -bonus);
}

inline void
CaptureHistory::update(Color c, PieceType piece, Square to, PieceType captured, int bonus) {
    const int piece_slot    = mo_detail::slot(piece);
    const int captured_slot = mo_detail::slot(captured);
    mo_detail::apply_signed_gravity(table[c][piece_slot][to][captured_slot], MAX_SCORE, bonus);
}

inline void CaptureHistory::age() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int piece = 0; piece < mo_detail::PIECE_SLOTS; ++piece) {
            for (int to = 0; to < N_SQUARES; ++to) {
                for (int captured = 0; captured < mo_detail::PIECE_SLOTS; ++captured) {
                    table[c][piece][to][captured] /= 2;
                }
            }
        }
    }
}

inline void CaptureHistory::clear() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int piece = 0; piece < mo_detail::PIECE_SLOTS; ++piece) {
            for (int to = 0; to < N_SQUARES; ++to) {
                for (int captured = 0; captured < mo_detail::PIECE_SLOTS; ++captured) {
                    table[c][piece][to][captured] = 0;
                }
            }
        }
    }
}

/*
 * Continuation history scores a quiet move in the context of the previous move.
 * It augments base quiet history for generated quiet ordering.
 */
struct ContinuationHistory {
    static constexpr int MAX_SCORE = mo_detail::MAX_SCORE;

    ContinuationHistory();

    int get(Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to) const;

    void reward(
        Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to, int depth);

    void penalize(Color     prev_c,
                  PieceType prev_piece,
                  Square    prev_to,
                  PieceType piece,
                  Square    to,
                  int       depth,
                  int       divisor = 1);

    void age();
    void clear();

private:
    static constexpr int ColorCount   = static_cast<int>(N_COLORS);
    static constexpr int SquareCount  = static_cast<int>(N_SQUARES);
    static constexpr int PrevKeyCount = ColorCount * mo_detail::PIECE_SLOTS * SquareCount;
    static constexpr int MoveKeyCount = mo_detail::PIECE_SLOTS * SquareCount;
    static constexpr int TableSize    = PrevKeyCount * MoveKeyCount;

    using Table = std::array<mo_detail::Score, TableSize>;

    void update(
        Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to, int bonus);

    static int
    index(Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to);

    std::unique_ptr<Table> table;
};

inline ContinuationHistory::ContinuationHistory() : table(std::make_unique<Table>()) {
    clear();
}

inline int ContinuationHistory::index(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to) {
    const int prev_piece_key =
        static_cast<int>(prev_c) * mo_detail::PIECE_SLOTS + mo_detail::slot(prev_piece);
    const int prev_key = prev_piece_key * SquareCount + static_cast<int>(prev_to);
    const int move_key = mo_detail::slot(piece) * SquareCount + static_cast<int>(to);

    return prev_key * MoveKeyCount + move_key;
}

inline int ContinuationHistory::get(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to) const {
    const int key = index(prev_c, prev_piece, prev_to, piece, to);
    return (*table)[key];
}

inline void ContinuationHistory::reward(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to, int depth) {
    const int bonus = std::clamp(depth * depth, 0, MAX_SCORE);
    update(prev_c, prev_piece, prev_to, piece, to, bonus);
}

inline void ContinuationHistory::penalize(Color     prev_c,
                                          PieceType prev_piece,
                                          Square    prev_to,
                                          PieceType piece,
                                          Square    to,
                                          int       depth,
                                          int       divisor) {
    const int safe_divisor = std::max(1, divisor);
    const int bonus        = std::clamp(depth * depth / safe_divisor, 0, MAX_SCORE);
    update(prev_c, prev_piece, prev_to, piece, to, -bonus);
}

inline void ContinuationHistory::update(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to, int bonus) {
    const int key = index(prev_c, prev_piece, prev_to, piece, to);
    mo_detail::apply_signed_gravity((*table)[key], MAX_SCORE, bonus);
}

inline void ContinuationHistory::age() {
    for (mo_detail::Score& entry : *table)
        entry /= 2;
}

inline void ContinuationHistory::clear() {
    std::fill(table->begin(), table->end(), mo_detail::Score{0});
}

/*
 * MoveOrdering owns the active per-worker move-ordering tables and exposes the
 * shared node context used by the picker and search history updates.
 */
struct MoveOrdering {
    /*
     * Context caches node-local keys derived from the board, avoiding repeated
     * previous-move extraction in hot move-ordering paths.
     */
    struct Context {
        Color     c{WHITE};
        Color     prev_c{WHITE};
        PieceType prev_piece{NO_PIECETYPE};
        Square    prev_to{INVALID};
        bool      has_prev{false};
    };

    KillerMoves         killers;
    CounterMoves        counters;
    QuietHistory        quiets;
    ContinuationHistory continuations;

    static Context make_context(const Board& board, bool include_prev = true);

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
    return context.has_prev ? counters.get(context.prev_c, context.prev_piece, context.prev_to)
                            : NULL_MOVE;
}

inline void MoveOrdering::update_quiet_refutations(const Context& context, Move move, int ply) {
    killers.update(move, ply);

    if (context.has_prev)
        counters.update(context.prev_c, context.prev_piece, context.prev_to, move);
}

inline int MoveOrdering::quiet_score(const Context& context,
                                     const Board&   board,
                                     Move           move,
                                     bool           include_continuation) const {
    const Square from  = move.from();
    const Square to    = move.to();
    int          score = quiets.get(context.c, from, to);

    if (!include_continuation || !context.has_prev)
        return score;

    const PieceType piece = moving_piece(board, move);
    if (piece != NO_PIECETYPE)
        score += continuations.get(context.prev_c, context.prev_piece, context.prev_to, piece, to);

    return score;
}

inline void
MoveOrdering::reward_quiet(const Context& context, const Board& board, Move move, int depth) {
    const Square from = move.from();
    const Square to   = move.to();
    quiets.reward(context.c, from, to, depth);

    if (!context.has_prev)
        return;

    const PieceType piece = moving_piece(board, move);
    if (piece != NO_PIECETYPE)
        continuations.reward(context.prev_c, context.prev_piece, context.prev_to, piece, to, depth);
}

inline void MoveOrdering::penalize_quiet(
    const Context& context, const Board& board, Move move, int depth, int divisor) {
    const Square from = move.from();
    const Square to   = move.to();
    quiets.penalize(context.c, from, to, depth, divisor);

    if (!context.has_prev)
        return;

    const PieceType piece = moving_piece(board, move);
    if (piece != NO_PIECETYPE)
        continuations.penalize(
            context.prev_c, context.prev_piece, context.prev_to, piece, to, depth, divisor);
}

inline MoveOrdering::Context MoveOrdering::make_context(const Board& board, bool include_prev) {
    Context context{.c = board.side_to_move()};

    if (!include_prev)
        return context;

    const Move prev_move = board.position_state().previous_move;
    if (prev_move.is_null())
        return context;

    const PieceType prev_piece =
        prev_move.type() == MOVE_PROM ? PAWN : type_of(board.piece_on(prev_move.to()));
    if (prev_piece == NO_PIECETYPE)
        return context;

    context.prev_c     = ~context.c;
    context.prev_piece = prev_piece;
    context.prev_to    = prev_move.to();
    context.has_prev   = true;
    return context;
}

inline PieceType MoveOrdering::moving_piece(const Board& board, Move move) {
    return type_of(board.piece_on(move.from()));
}
