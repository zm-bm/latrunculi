#pragma once

#include <chrono>

#include "constants.hpp"
#include "types.hpp"

const int TIME_NOT_SET = -1;

struct ParseWarning {
    std::string name;
    std::string value;
};

struct SearchOptions {
    std::string fen = STARTFEN;
    bool debug      = DEFAULT_DEBUG;
    int depth       = MAX_DEPTH;
    int movetime    = INT32_MAX;
    int nodes       = INT32_MAX;
    int wtime       = TIME_NOT_SET;
    int btime       = TIME_NOT_SET;
    int winc        = 0;
    int binc        = 0;
    int movestogo   = 30;

    std::vector<ParseWarning> warnings;

    SearchOptions() = default;

    SearchOptions(std::istringstream& iss) {
        std::string token;
        while (iss >> token) {
            int* target = nullptr;
            int min = INT32_MIN, max = INT32_MAX;

            if (token == "depth") {
                target = &depth;
                min    = 1;
                max    = MAX_DEPTH;
            } else if (token == "movetime") {
                target = &movetime;
                min    = 1;
                max    = INT32_MAX;
            } else if (token == "nodes") {
                target = &nodes;
                min    = 0;
                max    = INT32_MAX;
            } else if (token == "wtime") {
                target = &wtime;
                min    = 0;
                max    = INT32_MAX;
            } else if (token == "btime") {
                target = &btime;
                min    = 0;
                max    = INT32_MAX;
            } else if (token == "winc") {
                target = &winc;
                min    = 0;
                max    = INT32_MAX;
            } else if (token == "binc") {
                target = &binc;
                min    = 0;
                max    = INT32_MAX;
            } else if (token == "movestogo") {
                target = &movestogo;
                min    = 1;
                max    = INT32_MAX;
            }

            if (target) {
                int val;
                if (!(iss >> val)) {
                    iss.clear();
                    std::string bad;
                    iss >> bad;
                    warnings.push_back({token, bad});
                } else if (val < min || val > max) {
                    warnings.push_back({token, std::to_string(val)});
                    *target = std::clamp(val, min, max);
                } else {
                    *target = val;
                }
            }
        }
    }
};
