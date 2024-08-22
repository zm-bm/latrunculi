#include "globals.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include "types.hpp"
#include "magics.hpp"
#include "zobrist.hpp"

namespace G
{
    // 2d bitboards
    U64 BITS_BETWEEN[64][64];
    U64 BITS_INLINE[64][64];
}

void G::initBitboards() {
    // Initialize other bit masks
    for (Square sq1 = A1; sq1 != INVALID; sq1++) {
        for (Square sq2 = A1; sq2 != INVALID; sq2++) {
            // int rankDistance = abs(Types::getRank(sq1) - Types::getRank(sq2));
            // int fileDistance = abs(Types::getFile(sq1) - Types::getFile(sq2));
            // DISTANCE[sq1][sq2] = std::max(rankDistance, fileDistance);

            if (Magic::getBishopAttacks(sq1, 0) & BITSET[sq2]) {
                BITS_BETWEEN[sq1][sq2] = (
                    Magic::getBishopAttacks(sq1, BITSET[sq2])
                    & Magic::getBishopAttacks(sq2, BITSET[sq1])
                );
                BITS_INLINE[sq1][sq2] = (
                    (Magic::getBishopAttacks(sq1, 0) & Magic::getBishopAttacks(sq2, 0))
                    | BITSET[sq1]
                    | BITSET[sq2]
                );
            }

            if (Magic::getRookAttacks(sq1, 0) & BITSET[sq2]) {
                BITS_BETWEEN[sq1][sq2] = (
                    Magic::getRookAttacks(sq1, BITSET[sq2])
                    & Magic::getRookAttacks(sq2, BITSET[sq1])
                );
                BITS_INLINE[sq1][sq2] = (
                    (Magic::getRookAttacks(sq1, 0) & Magic::getRookAttacks(sq2, 0))
                    | BITSET[sq1]
                    | BITSET[sq2]
                );
            }
        }
    }
}

void G::init() {
    // Initialize magics and Zobrist keys
    Magic::init();
    Zobrist::init();
    G::initBitboards();
}

std::vector<std::string> G::split(const std::string &s, char delim)
{
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(s);

    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }

    return tokens;
}
