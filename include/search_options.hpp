#pragma once

#include <sstream>

#include "board.hpp"
#include "defs.hpp"

const int OPTION_NOT_SET = -1;

struct SearchOptions {
    Board*    board     = nullptr;
    TimePoint starttime = Clock::now();
    int       depth     = MAX_DEPTH;
    int       movetime  = OPTION_NOT_SET;
    int       nodes     = OPTION_NOT_SET;
    int       wtime     = OPTION_NOT_SET;
    int       btime     = OPTION_NOT_SET;
    int       winc      = OPTION_NOT_SET;
    int       binc      = OPTION_NOT_SET;
    int       movestogo = OPTION_NOT_SET;

    SearchOptions() = default;
    SearchOptions(std::istringstream& iss, Board* board = nullptr);

    void set_depth(int d) { depth = std::clamp(d, 1, MAX_DEPTH); }
    void set_movetime(int mt) { movetime = std::max(mt, 1); }
    void set_nodes(int n) { nodes = std::max(n, 0); }
    void set_wtime(int wt) { wtime = std::max(wt, 0); }
    void set_btime(int bt) { btime = std::max(bt, 0); }
    void set_winc(int wi) { winc = std::max(wi, 0); }
    void set_binc(int bi) { binc = std::max(bi, 0); }
    void set_movestogo(int mtg) { movestogo = std::max(mtg, 1); }

    int64_t calc_searchtime_ms(Color c) const;
};
