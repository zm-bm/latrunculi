#pragma once

#include "constants.hpp"

struct UCIOptions {
    int threads  = DEFAULT_THREADS;
    U64 hashSize = DEFAULT_HASH_SIZE;
    bool debug   = DEFAULT_DEBUG;
};
