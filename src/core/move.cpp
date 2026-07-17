#include "core/move.hpp"

#include <format>
#include <ostream>

#include "core/notation.hpp"

// Streams the same UCI-style text returned by Move::str().
std::ostream& operator<<(std::ostream& os, const Move& move) {
    os << move.str();
    return os;
}

// Formats a move as UCI text; NULL_MOVE is printed as "none".
std::string Move::str() const {
    if (is_null())
        return "none";

    return type() == MOVE_PROM
               ? std::format("{}{}{}", to_string(from()), to_string(to()), to_char(prom_piece()))
               : std::format("{}{}", to_string(from()), to_string(to()));
}
