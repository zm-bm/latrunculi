#pragma once

#ifndef LATRUNCULI_VERSION
#define LATRUNCULI_VERSION "0.0.0"
#endif

namespace engine {

constexpr const char* version = LATRUNCULI_VERSION;

constexpr int max_search_depth = 64;
constexpr int max_search_ply   = 2 * max_search_depth;

} // namespace engine
