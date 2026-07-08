#pragma once

#include <cstdint>
#include <limits>

#include "core/types.hpp"

#ifndef LATRUNCULI_VERSION
#define LATRUNCULI_VERSION "0.0.0"
#endif

namespace engine {

constexpr const char* version = LATRUNCULI_VERSION;

constexpr int max_search_depth = 64;
constexpr int max_search_ply   = 2 * max_search_depth;

} // namespace engine

namespace eval_value {

constexpr EvalValue draw          = 0;
constexpr EvalValue inf           = std::numeric_limits<std::int16_t>::max();
constexpr EvalValue mate          = inf - 1;
constexpr EvalValue mate_bound    = mate - engine::max_search_ply;
constexpr EvalValue tt_mate_bound = mate - 2 * engine::max_search_ply;

} // namespace eval_value

namespace piece_value {

constexpr EvalValue pawn_mg   = 100;
constexpr EvalValue knight_mg = 630;
constexpr EvalValue bishop_mg = 660;
constexpr EvalValue rook_mg   = 1000;
constexpr EvalValue queen_mg  = 2000;

constexpr EvalValue pawn_eg   = 166;
constexpr EvalValue knight_eg = 680;
constexpr EvalValue bishop_eg = 740;
constexpr EvalValue rook_eg   = 1100;
constexpr EvalValue queen_eg  = 2150;

} // namespace piece_value
