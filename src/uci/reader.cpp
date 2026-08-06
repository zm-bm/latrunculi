#include "uci/reader.hpp"

#include <istream>
#include <string>

#include "uci/parser.hpp"

namespace uci {

std::optional<Command> Reader::read_command() {
    std::string line;
    if (!std::getline(input, line))
        return std::nullopt;
    return parse_command(line);
}

} // namespace uci
