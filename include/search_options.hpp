#pragma once

#include <chrono>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>

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

        std::unordered_map<std::string, std::tuple<int*, int, int>> optionMap = {
            {"depth", {&depth, 1, MAX_DEPTH}},
            {"movetime", {&movetime, 1, INT32_MAX}},
            {"nodes", {&nodes, 0, INT32_MAX}},
            {"wtime", {&wtime, 0, INT32_MAX}},
            {"btime", {&btime, 0, INT32_MAX}},
            {"winc", {&winc, 0, INT32_MAX}},
            {"binc", {&binc, 0, INT32_MAX}},
            {"movestogo", {&movestogo, 1, INT32_MAX}}};

        while (iss >> token) {
            auto it = optionMap.find(token);

            if (it != optionMap.end()) {
                int* target = std::get<0>(it->second);
                int min     = std::get<1>(it->second);
                int max     = std::get<2>(it->second);
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
