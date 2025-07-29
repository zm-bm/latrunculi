#include "search_options.hpp"

#include "board.hpp"
#include "defs.hpp"
#include <sstream>

SearchOptions::SearchOptions(std::istringstream& iss, Board* board) : board(board) {
    std::string token;

    while (iss >> token) {
        int value;

        if (token == "depth" && iss >> value) {
            set_depth(value);
        } else if (token == "movetime" && iss >> value) {
            set_movetime(value);
        } else if (token == "nodes" && iss >> value) {
            set_nodes(value);
        } else if (token == "wtime" && iss >> value) {
            set_wtime(value);
        } else if (token == "btime" && iss >> value) {
            set_btime(value);
        } else if (token == "winc" && iss >> value) {
            set_winc(value);
        } else if (token == "binc" && iss >> value) {
            set_binc(value);
        } else if (token == "movestogo" && iss >> value) {
            set_movestogo(value);
        } else {
            iss.clear();
        }
    }
}

int64_t SearchOptions::calc_searchtime_ms(Color c) const {
    if (movetime != OPTION_NOT_SET) {
        return movetime;
    } else if (wtime != OPTION_NOT_SET && btime != OPTION_NOT_SET) {
        auto time  = (c == WHITE) ? wtime : btime;
        auto inc   = (c == WHITE) ? winc : binc;
        auto moves = (movestogo != OPTION_NOT_SET) ? movestogo : 30;
        auto res   = (time / moves) + inc - 50; // Subtract a small buffer
        return std::max(10, res);
    }
    return OPTION_NOT_SET;
}
