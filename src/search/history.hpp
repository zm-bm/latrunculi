#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

#include "core/piece.hpp"
#include "core/square.hpp"

namespace history {

// Compact signed entry type for the bounded history tables.
using Score = std::int16_t;

inline constexpr int max_score = 1024;

// Convert search depth into a bounded quadratic update amount.
inline int depth_bonus(int depth, int divisor = 1) {
    const int safe_divisor = std::max(1, divisor);
    return std::clamp(depth * depth / safe_divisor, 0, max_score);
}

// Apply bounded gravity so entries approach saturation smoothly.
inline void apply_gravity(Score& entry, int adjustment) {
    adjustment        = std::clamp(adjustment, -max_score, max_score);
    const int current = entry;
    const int weight  = adjustment < 0 ? -adjustment : adjustment;
    const int gravity = adjustment - (current * weight / max_score);

    entry = std::clamp(current + gravity, -max_score, max_score);
}

// Shared mutation policy used by the concrete history tables.
inline void reward_entry(Score& entry, int depth) {
    apply_gravity(entry, depth_bonus(depth));
}

inline void penalize_entry(Score& entry, int depth, int divisor = 1) {
    apply_gravity(entry, -depth_bonus(depth, divisor));
}

inline void age_entry(Score& entry) {
    entry /= 2;
}

} // namespace history

/*
 * Quiet history scores quiet moves by color, from, and to. Search rewards quiet
 * cutoffs and applies bounded maluses to failed ordinary quiets.
 */
struct QuietHistory {
    static constexpr int max_score = history::max_score;

    int  get(Color c, Square from, Square to) const;
    void reward(Color c, Square from, Square to, int depth);
    void penalize(Color c, Square from, Square to, int depth, int divisor = 1);
    void age();
    void clear();

private:
    history::Score&       entry(Color c, Square from, Square to);
    const history::Score& entry(Color c, Square from, Square to) const;

    history::Score table[N_COLORS][N_SQUARES][N_SQUARES] = {0};
};

inline int QuietHistory::get(Color c, Square from, Square to) const {
    return entry(c, from, to);
}

inline void QuietHistory::reward(Color c, Square from, Square to, int depth) {
    history::reward_entry(entry(c, from, to), depth);
}

inline void QuietHistory::penalize(Color c, Square from, Square to, int depth, int divisor) {
    history::penalize_entry(entry(c, from, to), depth, divisor);
}

inline history::Score& QuietHistory::entry(Color c, Square from, Square to) {
    return table[c][from][to];
}

inline const history::Score& QuietHistory::entry(Color c, Square from, Square to) const {
    return table[c][from][to];
}

inline void QuietHistory::age() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int from = 0; from < N_SQUARES; ++from) {
            for (int to = 0; to < N_SQUARES; ++to) {
                history::age_entry(table[c][from][to]);
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
    static constexpr int max_score = history::max_score;

    int  get(Color c, PieceType piece, Square to, PieceType captured) const;
    void reward(Color c, PieceType piece, Square to, PieceType captured, int depth);
    void
    penalize(Color c, PieceType piece, Square to, PieceType captured, int depth, int divisor = 1);
    void age();
    void clear();

private:
    history::Score&       entry(Color c, PieceType piece, Square to, PieceType captured);
    const history::Score& entry(Color c, PieceType piece, Square to, PieceType captured) const;

    history::Score table[N_COLORS][piece_slots][N_SQUARES][piece_slots] = {0};
};

inline int CaptureHistory::get(Color c, PieceType piece, Square to, PieceType captured) const {
    return entry(c, piece, to, captured);
}

inline void
CaptureHistory::reward(Color c, PieceType piece, Square to, PieceType captured, int depth) {
    history::reward_entry(entry(c, piece, to, captured), depth);
}

inline void CaptureHistory::penalize(
    Color c, PieceType piece, Square to, PieceType captured, int depth, int divisor) {
    history::penalize_entry(entry(c, piece, to, captured), depth, divisor);
}

inline history::Score&
CaptureHistory::entry(Color c, PieceType piece, Square to, PieceType captured) {
    const int piece_idx    = piece_slot(piece);
    const int captured_idx = piece_slot(captured);
    return table[c][piece_idx][to][captured_idx];
}

inline const history::Score&
CaptureHistory::entry(Color c, PieceType piece, Square to, PieceType captured) const {
    const int piece_idx    = piece_slot(piece);
    const int captured_idx = piece_slot(captured);
    return table[c][piece_idx][to][captured_idx];
}

inline void CaptureHistory::age() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int piece = 0; piece < piece_slots; ++piece) {
            for (int to = 0; to < N_SQUARES; ++to) {
                for (int captured = 0; captured < piece_slots; ++captured) {
                    history::age_entry(table[c][piece][to][captured]);
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
    static constexpr int max_score = history::max_score;

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

    using Table = std::array<history::Score, TableSize>;

    history::Score&
    entry(Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to);
    const history::Score&
    entry(Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to) const;

    static int
    index(Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to);

    // Heap-backed to keep MoveOrdering/worker storage small.
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
    return entry(prev_c, prev_piece, prev_to, piece, to);
}

inline void ContinuationHistory::reward(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to, int depth) {
    history::reward_entry(entry(prev_c, prev_piece, prev_to, piece, to), depth);
}

inline void ContinuationHistory::penalize(Color     prev_c,
                                          PieceType prev_piece,
                                          Square    prev_to,
                                          PieceType piece,
                                          Square    to,
                                          int       depth,
                                          int       divisor) {
    history::penalize_entry(entry(prev_c, prev_piece, prev_to, piece, to), depth, divisor);
}

inline history::Score& ContinuationHistory::entry(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to) {
    const int key = index(prev_c, prev_piece, prev_to, piece, to);
    return (*table)[key];
}

inline const history::Score& ContinuationHistory::entry(
    Color prev_c, PieceType prev_piece, Square prev_to, PieceType piece, Square to) const {
    const int key = index(prev_c, prev_piece, prev_to, piece, to);
    return (*table)[key];
}

inline void ContinuationHistory::age() {
    for (history::Score& entry : *table)
        history::age_entry(entry);
}

inline void ContinuationHistory::clear() {
    std::fill(table->begin(), table->end(), history::Score{0});
}
