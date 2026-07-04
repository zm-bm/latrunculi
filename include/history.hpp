#pragma once

#include <algorithm>

#include "defs.hpp"

struct QuietHistory {
    static constexpr int MAX_SCORE = 1024;

    int  get(Color c, Square from, Square to) const;
    void reward(Color c, Square from, Square to, int depth);
    void penalize(Color c, Square from, Square to, int depth, int divisor = 1);
    void age();
    void clear();

private:
    void update(Color c, Square from, Square to, int bonus);

    int16_t history[N_COLORS][N_SQUARES][N_SQUARES] = {0};
};

inline int QuietHistory::get(Color c, Square from, Square to) const {
    return history[c][from][to];
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
    bonus             = std::clamp(bonus, -MAX_SCORE, MAX_SCORE);
    const int current = history[c][from][to];
    const int weight  = bonus < 0 ? -bonus : bonus;
    const int gravity = bonus - (current * weight / MAX_SCORE);

    history[c][from][to] = std::clamp(current + gravity, -MAX_SCORE, MAX_SCORE);
}

inline void QuietHistory::age() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int from = 0; from < N_SQUARES; ++from) {
            for (int to = 0; to < N_SQUARES; ++to) {
                history[c][from][to] /= 2;
            }
        }
    }
}

inline void QuietHistory::clear() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int from = 0; from < N_SQUARES; ++from) {
            for (int to = 0; to < N_SQUARES; ++to) {
                history[c][from][to] = 0;
            }
        }
    }
}
