#pragma once

#include <string_view>

#include "uci/command.hpp"

namespace uci {

Command parse_command(std::string_view line);

} // namespace uci
