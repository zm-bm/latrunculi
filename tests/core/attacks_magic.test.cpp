#include "core/attacks_magic.hpp"

#include "core/bitboard.hpp"

#include <gtest/gtest.h>

TEST(MagicAttacksTest, ComputesBishopAttacks) {
    EXPECT_EQ(attacks::magic::bishop_moves(E4, 0),
              bb::set(B1, C2, D3, F5, G6, H7, H1, G2, F3, D5, C6, B7, A8));
    EXPECT_EQ(attacks::magic::bishop_moves(E4, bb::set(F5, D5, F3)),
              bb::set(B1, C2, D3, F5, D5, F3));
    EXPECT_EQ(attacks::magic::bishop_moves(E4, bb::set(F5, F3, D5, D3)), bb::set(F5, F3, D5, D3));
    EXPECT_EQ(attacks::magic::bishop_moves(A1, bb::set(C3)), bb::set(B2, C3));
}

TEST(MagicAttacksTest, ComputesRookAttacks) {
    EXPECT_EQ(attacks::magic::rook_moves(E4, 0),
              bb::set(E1, E2, E3, E5, E6, E7, E8, A4, B4, C4, D4, F4, G4, H4));
    EXPECT_EQ(attacks::magic::rook_moves(E4, bb::set(D4, E5, G4)),
              bb::set(D4, E5, E3, E2, E1, F4, G4));
    EXPECT_EQ(attacks::magic::rook_moves(E4, bb::set(D4, E5, E3, F4)), bb::set(D4, E5, E3, F4));
    EXPECT_EQ(attacks::magic::rook_moves(A1, bb::set(A4, B1)), bb::set(A2, A3, A4, B1));
}
