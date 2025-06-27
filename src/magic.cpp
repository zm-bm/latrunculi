// This file uses magic bitboard tables and constants from Pradyumna Kannan.
// See magic_tables.hpp/cpp for license and attribution details.

#include "magic.hpp"

#include "bb.hpp"
#include "magic_tables.hpp"
#include "types.hpp"

namespace Magic {

using namespace MagicTables;
using Direction = std::pair<int, int>;

// Translates line occupancy into an occupied-square bitboard
U64 calcOccupancy(std::vector<Square> squares, U64 occLine) {
    U64 ret = 0;

    for (size_t i = 0; i < squares.size(); i++) {
        // Check if the ith bit in occLine is set
        if (occLine & (1ULL << i)) {
            ret |= BB::set(squares[i]);
        }
    }

    return ret;
}

// Calculate moves for sliding pieces based on occupancy and directions
U64 calcSlidingMoves(int sq, U64 occupancy, const std::array<Direction, 4>& directions) {
    U64 attacks = 0ULL;

    // Iterate through each direction
    Square square = Square(sq);
    for (const auto [fDelta, rDelta] : directions) {
        File f = fileOf(square);
        Rank r = rankOf(square);

        // Slide with direction until we hit the board edge or an occupied square
        while (r >= Rank::R1 && r <= Rank::R8 && f >= File::F1 && f <= File::F8) {
            U64 currentSquare  = BB::set(makeSquare(f, r));
            attacks           |= currentSquare;
            if (occupancy & currentSquare) break;
            f = f + fDelta;
            r = r + rDelta;
        }
    }

    // Clear the starting square itself from the attack bitboard
    attacks &= BB::clear(square);

    return attacks;
}

template <typename MoveFunc>
void initializeMagicBitboards(const U64 magicMask[64],
                              const U64 magicNum[64],
                              const int Magichift[64],
                              U64* const attacks[64],
                              MoveFunc calculateMoves) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        U64 mask = magicMask[sq];

        // Find all squares in the mask
        std::vector<Square> squares;
        while (mask) squares.push_back(BB::lsbPop(mask));

        // Enumerate all occupancy variations and calculate attacks
        U64 lineEnd = 0x1ull << squares.size();
        for (U64 lineOccupancy = 0; lineOccupancy < lineEnd; lineOccupancy++) {
            U64 occupancy  = calcOccupancy(squares, lineOccupancy);
            U64 offset     = (occupancy * magicNum[sq]) >> Magichift[sq];
            U64& sqAttacks = *(attacks[sq] + offset);
            sqAttacks      = calculateMoves(sq, occupancy);
        }
    }
}

U64 calcBishopMoves(int sq, U64 occupancy) {
    std::array<Direction, 4> directions = {{{-1, 1}, {1, 1}, {-1, -1}, {1, -1}}};
    return calcSlidingMoves(sq, occupancy, directions);
}

U64 calcRookMoves(int sq, U64 occupancy) {
    std::array<Direction, 4> directions = {{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};
    return calcSlidingMoves(sq, occupancy, directions);
}

void init() {
    initializeMagicBitboards(bishopMask, bishopMagic, bishopShift, bishopAttacks, calcBishopMoves);
    initializeMagicBitboards(rookMask, rookMagic, rookShift, rookAttacks, calcRookMoves);
}

}  // namespace Magic
