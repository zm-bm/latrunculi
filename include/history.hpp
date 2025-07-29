#pragma once

#include "defs.hpp"

struct HistoryTable {
    static constexpr int MAX_HISTORY = PRIORITY_HISTORY;

    int16_t history[N_COLORS][N_SQUARES][N_SQUARES] = {0};

    int  get(Color c, Square from, Square to) const;
    void update(Color c, Square from, Square to, int depth);
    void age();
    void clear();
};

inline int HistoryTable::get(Color c, Square from, Square to) const {
    return history[c][from][to];
}

inline void HistoryTable::update(Color c, Square from, Square to, int depth) {
    int bonus   = depth * depth;
    int clamped = std::clamp(bonus, -MAX_HISTORY, MAX_HISTORY);
    int gravity = bonus - (history[c][from][to] * bonus / MAX_HISTORY);

    history[c][from][to] += gravity;
}

inline void HistoryTable::age() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int from = 0; from < N_SQUARES; ++from) {
            for (int to = 0; to < N_SQUARES; ++to) {
                history[c][from][to] >>= 1;
            }
        }
    }
}

inline void HistoryTable::clear() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int from = 0; from < N_SQUARES; ++from) {
            for (int to = 0; to < N_SQUARES; ++to) {
                history[c][from][to] = 0;
            }
        }
    }
}
