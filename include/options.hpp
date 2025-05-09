#pragma once

#include "constants.hpp"
#include "types.hpp"

struct Options {
    bool debug   = SEARCH_DEBUG;
    int depth    = SEARCH_DEPTH;
    int movetime = INT32_MAX;
};
