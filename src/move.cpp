#include "move.hpp"

#include <format>

std::ostream& operator<<(std::ostream& os, const Move& move) {
    os << move.str();
    return os;
}

std::string Move::str() const {
    if (is_null())
        return "none";

    return type() == MOVE_PROM
               ? std::format("{}{}{}", to_string(from()), to_string(to()), to_char(prom_piece()))
               : std::format("{}{}", to_string(from()), to_string(to()));
}
