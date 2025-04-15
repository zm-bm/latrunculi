#include "move.hpp"

std::string Move::str() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}