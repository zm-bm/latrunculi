#pragma once

#include <chrono>
#include <cstdint>

// Monotonic clock used by search timing.
using SearchClock  = std::chrono::steady_clock;
using TimePoint    = SearchClock::time_point;
using Milliseconds = std::chrono::milliseconds;

// Engine-wide scalar aliases.
using EvalValue   = std::int32_t;
using Bitboard    = std::uint64_t;
using PositionKey = std::uint64_t;
using NodeCount   = std::uint64_t;

// Board-coordinate types are defined in square.hpp.
enum Rank : std::int8_t;
enum File : std::int8_t;
enum Square : std::int8_t;

// Side-to-move/color encoding; WHITE is the low-bit toggle.
enum Color : std::uint8_t { BLACK, WHITE, N_COLORS };

// Opposite color.
constexpr Color operator~(Color c) {
    return static_cast<Color>(c ^ WHITE);
}
