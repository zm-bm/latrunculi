#pragma once

#include <cstdint>
#include <limits>

#include "core/types.hpp"

namespace engine {

// Build-provided engine version string.
constexpr const char* version = LATRUNCULI_VERSION;

// Default transposition-table capacity advertised through UCI, in megabytes.
constexpr int default_hash_mb = 32;

// Search-depth bounds used for fixed-size stacks and mate-distance margins.
constexpr int max_search_depth = 64;
constexpr int max_search_ply = 2 * max_search_depth;

} // namespace engine

namespace scores {

// Evaluation sentinels and mate-score guard bands.
constexpr Score draw = 0;
constexpr Score inf = std::numeric_limits<std::int16_t>::max();
constexpr Score mate = inf - 1;
constexpr Score mate_bound = mate - engine::max_search_ply;
constexpr Score tt_mate_bound = mate - 2 * engine::max_search_ply;

} // namespace scores
