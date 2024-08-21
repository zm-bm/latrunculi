#include "globals.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include "types.hpp"
#include "magics.hpp"
#include "zobrist.hpp"

namespace G
{
    // default fen / position
    const std::string STARTFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    // base bitboards
    U64 BITSET[64];
    U64 BITCLEAR[64];

    // 2d bitboards
    U64 BITS_BETWEEN[64][64];
    U64 BITS_INLINE[64][64];
    int DISTANCE[64][64];

    // attack bitboards
    U64 KNIGHT_ATTACKS[64];
    U64 KING_ATTACKS[64];

    const Rank RANK[2][8] = {
        { RANK8, RANK7, RANK6, RANK5, RANK4, RANK3, RANK2, RANK1 },
        { RANK1, RANK2, RANK3, RANK4, RANK5, RANK6, RANK7, RANK8 }
    };

    const File FILE[2][8] = {
        { FILE8, FILE7, FILE6, FILE5, FILE4, FILE3, FILE2, FILE1 },
        { FILE1, FILE2, FILE3, FILE4, FILE5, FILE6, FILE7, FILE8 }
    };

    const U64 RANK_MASK[8] = {
        0xFF,
        0xFF00,
        0xFF0000,
        0xFF000000,
        0xFF00000000,
        0xFF0000000000,
        0xFF000000000000,
        0xFF00000000000000
    };

    const U64 FILE_MASK[8] = {
        0x0101010101010101,
        0x202020202020202,
        0x404040404040404,
        0x808080808080808,
        0x1010101010101010,
        0x2020202020202020,
        0x4040404040404040,
        0x8080808080808080
    };

}

void addTarget(Square orig, File targetFile, Rank targetRank, U64 *arr) {
    auto square = Types::getSquare(targetFile, targetRank);
    if (Types::validFile(targetFile) && Types::validRank(targetRank)) {
        arr[orig] |= G::BITSET[square];
    }
}

void G::initBitboards() {
    for (Square sq = A1; sq != INVALID; sq++) {
        BITSET[sq] = 0x1ull << sq;
        BITCLEAR[sq] = ~BITSET[sq];
    }

    for (Square sq = A1; sq != INVALID; sq++) {
        auto file = Types::getFile(sq);
        auto rank = Types::getRank(sq);

        addTarget(sq, file + 2, rank + 1, KNIGHT_ATTACKS);
        addTarget(sq, file + 2, rank - 1, KNIGHT_ATTACKS);
        addTarget(sq, file - 2, rank + 1, KNIGHT_ATTACKS);
        addTarget(sq, file - 2, rank - 1, KNIGHT_ATTACKS);
        addTarget(sq, file + 1, rank + 2, KNIGHT_ATTACKS);
        addTarget(sq, file - 1, rank + 2, KNIGHT_ATTACKS);
        addTarget(sq, file + 1, rank - 2, KNIGHT_ATTACKS);
        addTarget(sq, file - 1, rank - 2, KNIGHT_ATTACKS);

        addTarget(sq, file - 1, rank - 1, KING_ATTACKS);
        addTarget(sq, file - 1, rank + 1, KING_ATTACKS);
        addTarget(sq, file + 1, rank - 1, KING_ATTACKS);
        addTarget(sq, file + 1, rank + 1, KING_ATTACKS);
        addTarget(sq, file,     rank - 1, KING_ATTACKS);
        addTarget(sq, file,     rank + 1, KING_ATTACKS);
        addTarget(sq, file - 1, rank,     KING_ATTACKS);
        addTarget(sq, file + 1, rank,     KING_ATTACKS);
    }

    // Initialize other bit masks
    for (Square sq1 = A1; sq1 != INVALID; sq1++) {
        for (Square sq2 = A1; sq2 != INVALID; sq2++) {
            int rankDistance = abs(Types::getRank(sq1) - Types::getRank(sq2));
            int fileDistance = abs(Types::getFile(sq1) - Types::getFile(sq2));
            DISTANCE[sq1][sq2] = std::max(rankDistance, fileDistance);

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
