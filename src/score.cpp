#include "score.hpp"

#include "constants.hpp"

std::ostream& operator<<(std::ostream& os, const Score& score) {
    os << std::setw(5) << double(score.mg) / PAWN_VALUE_MG << " ";
    os << std::setw(5) << double(score.eg) / PAWN_VALUE_MG;
    return os;
}
