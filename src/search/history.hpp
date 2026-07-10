#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

#include "core/piece.hpp"
#include "core/square.hpp"

using HistoryScore = std::int16_t;

inline constexpr int max_history_score = 1024;

inline void apply_history_gravity(HistoryScore& entry, int max_score, int bonus) {
    bonus             = std::clamp(bonus, -max_score, max_score);
    const int current = entry;
    const int weight  = bonus < 0 ? -bonus : bonus;
    const int gravity = bonus - (current * weight / max_score);

    entry = std::clamp(current + gravity, -max_score, max_score);
}

/*
 * Quiet history scores quiet moves by color, from, and to. Search rewards quiet
 * cutoffs and applies bounded maluses to failed ordinary quiets.
 */
struct QuietHistory {
    static constexpr int max_score = max_history_score;

    int  get(Color c, Square from, Square to) const;
    void reward(Color c, Square from, Square to, int depth);
    void penalize(Color c, Square from, Square to, int depth, int divisor = 1);
    void age();
    void clear();

private:
    void update(Color c, Square from, Square to, int bonus);

    HistoryScore table[N_COLORS][N_SQUARES][N_SQUARES] = {0};
};

inline int QuietHistory::get(Color c, Square from, Square to) const {
    return table[c][from][to];
}

inline void QuietHistory::reward(Color c, Square from, Square to, int depth) {
    const int bonus = std::clamp(depth * depth, 0, max_score);
    update(c, from, to, bonus);
}

inline void QuietHistory::penalize(Color c, Square from, Square to, int depth, int divisor) {
    const int safe_divisor = std::max(1, divisor);
    const int bonus        = std::clamp(depth * depth / safe_divisor, 0, max_score);
    update(c, from, to, -bonus);
}

inline void QuietHistory::update(Color c, Square from, Square to, int bonus) {
    apply_history_gravity(table[c][from][to], max_score, bonus);
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
    static constexpr int max_score = max_history_score;

    int  get(Color c, PieceType piece, Square to, PieceType captured) const;
    void reward(Color c, PieceType piece, Square to, PieceType captured, int depth);
    void
    penalize(Color c, PieceType piece, Square to, PieceType captured, int depth, int divisor = 1);
    void age();
    void clear();

private:
    void update(Color c, PieceType piece, Square to, PieceType captured, int bonus);

    HistoryScore table[N_COLORS][piece_slots][N_SQUARES][piece_slots] = {0};
};

inline int CaptureHistory::get(Color c, PieceType piece, Square to, PieceType captured) const {
    const int piece_idx    = piece_slot(piece);
    const int captured_idx = piece_slot(captured);
    return table[c][piece_idx][to][captured_idx];
}

inline void
CaptureHistory::reward(Color c, PieceType piece, Square to, PieceType captured, int depth) {
    const int bonus = std::clamp(depth * depth, 0, max_score);
    update(c, piece, to, captured, bonus);
}

inline void CaptureHistory::penalize(
    Color c, PieceType piece, Square to, PieceType captured, int depth, int divisor) {
    const int safe_divisor = std::max(1, divisor);
    const int bonus        = std::clamp(depth * depth / safe_divisor, 0, max_score);
    update(c, piece, to, captured, -bonus);
}

inline void
CaptureHistory::update(Color c, PieceType piece, Square to, PieceType captured, int bonus) {
    const int piece_idx    = piece_slot(piece);
    const int captured_idx = piece_slot(captured);
    apply_history_gravity(table[c][piece_idx][to][captured_idx], max_score, bonus);
}

inline void CaptureHistory::age() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int piece = 0; piece < piece_slots; ++piece) {
            for (int to = 0; to < N_SQUARES; ++to) {
                for (int captured = 0; captured < piece_slots; ++captured) {
                    table[c][piece][to][captured] /= 2;
                }
            }
        }
    }
}

inline void CaptureHistory::clear() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int piece = 0; piece < piece_slots; ++piece) {
            for (int to = 0; to < N_SQUARES; ++to) {
                for (int captured = 0; captured < piece_slots; ++captured) {
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
    static constexpr int max_score = max_history_score;

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
    static constexpr int PrevKeyCount = ColorCount * piece_slots * SquareCount;
    static constexpr int MoveKeyCount = piece_slots * SquareCount;
    static constexpr int TableSize    = PrevKeyCount * MoveKeyCount;

    using Table = std::array<HistoryScore, TableSize>;

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
    const int prev_piece_key = static_cast<int>(prev_c) * piece_slots + piece_slot(prev_piece);
    const int prev_key       = prev_piece_key * SquareCount + static_cast<int>(prev_to);
    const int move_key       = piece_slot(piece) * SquareCount + static_cast<int>(to);

    return prev_key * MoveKeyCount + move_key;
}

inline int ContinuationHistory::get(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to) const {
    const int key = index(prev_c, prev_piece, prev_to, piece, to);
    return (*table)[key];
}

inline void ContinuationHistory::reward(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to, int depth) {
    const int bonus = std::clamp(depth * depth, 0, max_score);
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
    const int bonus        = std::clamp(depth * depth / safe_divisor, 0, max_score);
    update(prev_c, prev_piece, prev_to, piece, to, -bonus);
}

inline void ContinuationHistory::update(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to, int bonus) {
    const int key = index(prev_c, prev_piece, prev_to, piece, to);
    apply_history_gravity((*table)[key], max_score, bonus);
}

inline void ContinuationHistory::age() {
    for (HistoryScore& entry : *table)
        entry /= 2;
}

inline void ContinuationHistory::clear() {
    std::fill(table->begin(), table->end(), HistoryScore{0});
}
