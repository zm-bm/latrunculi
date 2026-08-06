#pragma once

#include <iosfwd>
#include <optional>

#include "uci/command.hpp"

namespace uci {

class Reader {
public:
    explicit Reader(std::istream& input_stream) : input(input_stream) {}

    std::optional<Command> read_command();

private:
    std::istream& input;
};

} // namespace uci
