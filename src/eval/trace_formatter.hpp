#pragma once

#include <string>

#include "eval/trace.hpp"

namespace eval {

[[nodiscard]] std::string format_trace(const Trace& trace);

} // namespace eval
