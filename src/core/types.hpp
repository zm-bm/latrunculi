#pragma once

#include <chrono>
#include <cstdint>

using SearchClock  = std::chrono::steady_clock;
using TimePoint    = SearchClock::time_point;
using Milliseconds = std::chrono::milliseconds;
using EvalValue    = std::int32_t;
using Bitboard     = std::uint64_t;
using PositionKey  = std::uint64_t;
using NodeCount    = std::uint64_t;

enum Rank : std::int8_t;
enum File : std::int8_t;
enum Square : std::int8_t;

enum Color : std::uint8_t { BLACK, WHITE, N_COLORS };

constexpr Color operator~(Color c) {
    return static_cast<Color>(c ^ WHITE);
}
