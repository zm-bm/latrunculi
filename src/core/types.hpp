#pragma once

#include <chrono>
#include <cstdint>

using SearchClock  = std::chrono::steady_clock;
using TimePoint    = SearchClock::time_point;
using Milliseconds = std::chrono::milliseconds;
using EvalValue    = std::int32_t;

enum Rank : int8_t;
enum File : int8_t;
enum Square : int8_t;

enum Color : uint8_t { BLACK, WHITE, N_COLORS };

constexpr Color operator~(Color c) {
    return static_cast<Color>(c ^ WHITE);
}
