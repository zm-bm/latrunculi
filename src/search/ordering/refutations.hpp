#pragma once

#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"

namespace search::ordering {

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

} // namespace search::ordering
