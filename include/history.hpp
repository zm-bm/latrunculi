#pragma once

#include <algorithm>
#include <cstdint>

#include "defs.hpp"

namespace history_detail {

inline void apply_signed_gravity(int16_t& entry, int max_score, int bonus) {
    bonus             = std::clamp(bonus, -max_score, max_score);
    const int current = entry;
    const int weight  = bonus < 0 ? -bonus : bonus;
    const int gravity = bonus - (current * weight / max_score);

    entry = std::clamp(current + gravity, -max_score, max_score);
}

} // namespace history_detail

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
