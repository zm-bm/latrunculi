#pragma once

#include <chrono>

#include "constants.hpp"
#include "types.hpp"

struct SearchOptions {
    std::string fen = STARTFEN;
    bool debug      = DEFAULT_DEBUG;
    int depth       = DEFAULT_DEPTH;
    int movetime    = INT32_MAX;
};
