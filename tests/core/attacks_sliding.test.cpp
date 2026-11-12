#include "core/attacks_sliding.hpp"

#include "core/bitboard.hpp"

#include <array>
#include <utility>

#include <gtest/gtest.h>

namespace {

using Directions = std::array<std::pair<int, int>, 4>;

Bitboard ray_attacks(Square from, Bitboard occupied, const Directions& directions) {
    Bitboard result = 0;
    for (const auto& [df, dr] : directions) {
        int file = square::file_of(from);
        int rank = square::rank_of(from);
        while (true) {
            file += df;
            rank += dr;
            if (file < 0 || file >= 8 || rank < 0 || rank >= 8)
                break;
            const Bitboard target = bb::set(square::make(File(file), Rank(rank)));
            result |= target;
            if (occupied & target)
                break;
        }
    }
    return result;
}

void check_all_occupancies(Bitboard (*lookup)(Square, Bitboard), const Directions& directions) {
    for (Square sq = A1; sq < INVALID; ++sq) {
        SCOPED_TRACE(static_cast<int>(sq));
        // Derive relevant blockers geometrically, independently of lookup metadata.
        const Bitboard edges =
            ((bb::rank(RANK1) | bb::rank(RANK8)) & ~bb::rank(square::rank_of(sq)))
            | ((bb::file(FILE1) | bb::file(FILE8)) & ~bb::file(square::file_of(sq)));
        const Bitboard mask = ray_attacks(sq, 0, directions) & ~edges;
        Bitboard occupied = 0;
        do {
            const Bitboard expected = ray_attacks(sq, occupied, directions);
            ASSERT_EQ(lookup(sq, occupied), expected) << "occupancy: " << occupied;
            // All bits excluded from the index must leave attacks unchanged.
            ASSERT_EQ(lookup(sq, occupied | ~mask), ray_attacks(sq, occupied | ~mask, directions))
                << "occupancy: "
                << occupied;
            occupied = (occupied - mask) & mask;
        } while (occupied);
    }
}

} // namespace

TEST(SlidingAttacksTest, EveryBishopOccupancyMatchesRayWalk) {
    check_all_occupancies(attacks::sliding::bishop_moves, {{{1, 1}, {-1, 1}, {1, -1}, {-1, -1}}});
}

TEST(SlidingAttacksTest, EveryRookOccupancyMatchesRayWalk) {
    check_all_occupancies(attacks::sliding::rook_moves, {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}});
}

TEST(SlidingAttacksTest, ComputesBishopAttacks) {
    EXPECT_EQ(attacks::sliding::bishop_moves(E4, 0),
              bb::set(B1, C2, D3, F5, G6, H7, H1, G2, F3, D5, C6, B7, A8));
    EXPECT_EQ(attacks::sliding::bishop_moves(E4, bb::set(F5, D5, F3)),
              bb::set(B1, C2, D3, F5, D5, F3));
    EXPECT_EQ(attacks::sliding::bishop_moves(E4, bb::set(F5, F3, D5, D3)), bb::set(F5, F3, D5, D3));
    EXPECT_EQ(attacks::sliding::bishop_moves(A1, bb::set(C3)), bb::set(B2, C3));
}

TEST(SlidingAttacksTest, ComputesRookAttacks) {
    EXPECT_EQ(attacks::sliding::rook_moves(E4, 0),
              bb::set(E1, E2, E3, E5, E6, E7, E8, A4, B4, C4, D4, F4, G4, H4));
    EXPECT_EQ(attacks::sliding::rook_moves(E4, bb::set(D4, E5, G4)),
              bb::set(D4, E5, E3, E2, E1, F4, G4));
    EXPECT_EQ(attacks::sliding::rook_moves(E4, bb::set(D4, E5, E3, F4)), bb::set(D4, E5, E3, F4));
    EXPECT_EQ(attacks::sliding::rook_moves(A1, bb::set(A4, B1)), bb::set(A2, A3, A4, B1));
}
