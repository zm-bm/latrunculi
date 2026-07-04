#include "search_options.hpp"

#include "board.hpp"
#include "defs.hpp"

int64_t SearchOptions::calc_searchtime_ms(Color c) const {
    if (movetime != OPTION_NOT_SET) {
        return movetime;
    } else if (wtime != OPTION_NOT_SET && btime != OPTION_NOT_SET) {
        auto time  = (c == WHITE) ? wtime : btime;
        auto inc   = (c == WHITE) ? winc : binc;
        inc        = (inc != OPTION_NOT_SET) ? inc : 0;
        auto moves = (movestogo != OPTION_NOT_SET) ? movestogo : 30;
        auto res   = (time / moves) + inc - 50; // Subtract a small buffer
        return std::max(10, res);
    }
    return OPTION_NOT_SET;
}
