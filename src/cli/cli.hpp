#pragma once

#include <iosfwd>

namespace cli {

int run(int           argument_count,
        char* const   arguments[],
        std::istream& input,
        std::ostream& output,
        std::ostream& diagnostics);

} // namespace cli
