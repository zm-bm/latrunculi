#pragma once

#include <chrono>

#include "constants.hpp"
#include "types.hpp"

struct Context {
    bool debug   = SEARCH_DEBUG;
    int depth    = SEARCH_DEPTH;
    int movetime = INT32_MAX;

    std::chrono::high_resolution_clock::time_point startTime;
};
