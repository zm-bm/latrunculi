#pragma once

#include <algorithm>
#include <cstdint>

#include "board.hpp"
#include "defs.hpp"
#include "move.hpp"

namespace history_detail {

inline void apply_signed_gravity(int16_t& entry, int max_score, int bonus) {
    bonus             = std::clamp(bonus, -max_score, max_score);
    const int current = entry;
    const int weight  = bonus < 0 ? -bonus : bonus;
    const int gravity = bonus - (current * weight / max_score);

    entry = std::clamp(current + gravity, -max_score, max_score);
}

} // namespace history_detail

struct KillerMoves {
    Move killers[MAX_SEARCH_PLY][2] = {NULL_MOVE};

    void update(Move killer, int ply);
    bool is_killer(Move move, int ply) const;
    Move primary(int ply) const;
    Move secondary(int ply) const;
    void clear();
};

inline void KillerMoves::update(Move killer, int ply) {
    if (killers[ply][0] == killer)
        return;
    killers[ply][1] = killers[ply][0];
    killers[ply][0] = killer;
}

inline bool KillerMoves::is_killer(Move move, int ply) const {
    return move == killers[ply][0] || move == killers[ply][1];
}

inline Move KillerMoves::primary(int ply) const {
    return killers[ply][0];
}

inline Move KillerMoves::secondary(int ply) const {
    return killers[ply][1];
}

inline void KillerMoves::clear() {
    for (int ply = 0; ply < MAX_SEARCH_PLY; ++ply) {
        killers[ply][0] = NULL_MOVE;
        killers[ply][1] = NULL_MOVE;
    }
}

struct CounterMoves {
    Move get(Color previous_mover, PieceType previous_piece, Square previous_to) const;
    void update(Color previous_mover, PieceType previous_piece, Square previous_to, Move counter);
    void clear();

private:
    Move table[N_COLORS][N_PIECES][N_SQUARES] = {NULL_MOVE};
};

inline Move
CounterMoves::get(Color previous_mover, PieceType previous_piece, Square previous_to) const {
    return table[previous_mover][previous_piece][previous_to];
}

inline void CounterMoves::update(Color     previous_mover,
                                 PieceType previous_piece,
                                 Square    previous_to,
                                 Move      counter) {
    table[previous_mover][previous_piece][previous_to] = counter;
}

inline void CounterMoves::clear() {
    for (int color = 0; color < N_COLORS; ++color) {
        for (int piece = 0; piece < N_PIECES; ++piece) {
            for (int to = 0; to < N_SQUARES; ++to) {
                table[color][piece][to] = NULL_MOVE;
            }
        }
    }
}

struct QuietHistory {
    static constexpr int MAX_SCORE = 1024;

    int  get(Color c, Square from, Square to) const;
    void reward(Color c, Square from, Square to, int depth);
    void penalize(Color c, Square from, Square to, int depth, int divisor = 1);
    void age();
    void clear();

private:
    void update(Color c, Square from, Square to, int bonus);

    int16_t table[N_COLORS][N_SQUARES][N_SQUARES] = {0};
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
    history_detail::apply_signed_gravity(table[c][from][to], MAX_SCORE, bonus);
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

struct CaptureHistory {
    static constexpr int MAX_SCORE = 1024;

    int  get(Color c, PieceType moved, Square to, PieceType captured) const;
    void reward(Color c, PieceType moved, Square to, PieceType captured, int depth);
    void
    penalize(Color c, PieceType moved, Square to, PieceType captured, int depth, int divisor = 1);
    void age();
    void clear();

private:
    void update(Color c, PieceType moved, Square to, PieceType captured, int bonus);

    int16_t table[N_COLORS][N_PIECES][N_SQUARES][N_PIECES] = {0};
};

inline int CaptureHistory::get(Color c, PieceType moved, Square to, PieceType captured) const {
    return table[c][moved][to][captured];
}

inline void
CaptureHistory::reward(Color c, PieceType moved, Square to, PieceType captured, int depth) {
    const int bonus = std::clamp(depth * depth, 0, MAX_SCORE);
    update(c, moved, to, captured, bonus);
}

inline void CaptureHistory::penalize(
    Color c, PieceType moved, Square to, PieceType captured, int depth, int divisor) {
    const int safe_divisor = std::max(1, divisor);
    const int bonus        = std::clamp(depth * depth / safe_divisor, 0, MAX_SCORE);
    update(c, moved, to, captured, -bonus);
}

inline void
CaptureHistory::update(Color c, PieceType moved, Square to, PieceType captured, int bonus) {
    history_detail::apply_signed_gravity(table[c][moved][to][captured], MAX_SCORE, bonus);
}

inline void CaptureHistory::age() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int moved = 0; moved < N_PIECES; ++moved) {
            for (int to = 0; to < N_SQUARES; ++to) {
                for (int captured = 0; captured < N_PIECES; ++captured) {
                    table[c][moved][to][captured] /= 2;
                }
            }
        }
    }
}

inline void CaptureHistory::clear() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int moved = 0; moved < N_PIECES; ++moved) {
            for (int to = 0; to < N_SQUARES; ++to) {
                for (int captured = 0; captured < N_PIECES; ++captured) {
                    table[c][moved][to][captured] = 0;
                }
            }
        }
    }
}

struct MoveOrdering {
    KillerMoves  killers;
    CounterMoves counters;
    QuietHistory quiets;

    void clear();
    bool is_killer(Move move, int ply) const;
    Move counter_hint(const Board& board) const;
    void update_quiet_refutations(const Board& board, Move move, int ply);

private:
    struct CounterKey {
        Color     previous_mover{WHITE};
        PieceType previous_piece{NO_PIECETYPE};
        Square    previous_to{INVALID};
        bool      valid{false};
    };

    static CounterKey counter_key(const Board& board);
};

inline void MoveOrdering::clear() {
    killers.clear();
    counters.clear();
    quiets.clear();
}

inline bool MoveOrdering::is_killer(Move move, int ply) const {
    return killers.is_killer(move, ply);
}

inline Move MoveOrdering::counter_hint(const Board& board) const {
    const CounterKey key = counter_key(board);
    return key.valid ? counters.get(key.previous_mover, key.previous_piece, key.previous_to)
                     : NULL_MOVE;
}

inline void MoveOrdering::update_quiet_refutations(const Board& board, Move move, int ply) {
    killers.update(move, ply);

    const CounterKey key = counter_key(board);
    if (key.valid)
        counters.update(key.previous_mover, key.previous_piece, key.previous_to, move);
}

inline MoveOrdering::CounterKey MoveOrdering::counter_key(const Board& board) {
    const Move previous_move = board.position_state().previous_move;
    if (previous_move.is_null())
        return {};

    const PieceType previous_piece =
        previous_move.type() == MOVE_PROM ? PAWN : type_of(board.piece_on(previous_move.to()));
    if (previous_piece == NO_PIECETYPE)
        return {};

    return CounterKey{
        .previous_mover = ~board.side_to_move(),
        .previous_piece = previous_piece,
        .previous_to    = previous_move.to(),
        .valid          = true,
    };
}
