#pragma once

#include <string>

namespace eval {

class Trace;

[[nodiscard]] std::string format_trace(const Trace& trace);

} // namespace eval
