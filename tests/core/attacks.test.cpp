#include "core/attacks.hpp"

#include <gtest/gtest.h>

TEST(AttacksTest, CorrectPawnShift) {
    Bitboard pawns = bb::set(D4);
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::push>(pawns, WHITE), bb::set(D5));
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::push>(pawns, BLACK), bb::set(D3));
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::left>(pawns, WHITE), bb::set(C5));
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::right>(pawns, WHITE), bb::set(E5));
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::left>(pawns, BLACK), bb::set(E3));
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::right>(pawns, BLACK), bb::set(C3));
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::double_push>(pawns, WHITE), bb::set(D6));
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::double_push>(pawns, BLACK), bb::set(D2));

    Bitboard pawns_left = bb::set(A4);
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::left>(pawns_left, WHITE), 0);
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::right>(pawns_left, BLACK), 0);

    Bitboard pawns_right = bb::set(H4);
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::right>(pawns_right, WHITE), 0);
    EXPECT_EQ(attacks::pawn_shift<pawn_delta::left>(pawns_right, BLACK), 0);
}

TEST(AttacksTest, ComputesPawnAndLeaperAttacks) {
    const Bitboard pawns         = bb::set(A4, D4, H4);
    const Bitboard white_attacks = bb::set(B5, C5, E5, G5);
    const Bitboard black_attacks = bb::set(B3, C3, E3, G3);
    EXPECT_EQ(attacks::pawn_attacks<WHITE>(pawns), white_attacks);
    EXPECT_EQ(attacks::pawn_attacks<BLACK>(pawns), black_attacks);
    EXPECT_EQ(attacks::pawn_attacks(pawns, WHITE), white_attacks);
    EXPECT_EQ(attacks::pawn_attacks(pawns, BLACK), black_attacks);

    EXPECT_EQ(attacks::piece_moves<KNIGHT>(A1), bb::set(B3, C2));
    EXPECT_EQ(attacks::piece_moves<KNIGHT>(C6), bb::set(A5, A7, B4, B8, D4, D8, E5, E7));

    EXPECT_EQ(attacks::piece_moves<KING>(A1), bb::set(A2, B1, B2));
    EXPECT_EQ(attacks::piece_moves<KING>(G2), bb::set(F1, F2, F3, G1, G3, H1, H2, H3));
}

TEST(AttacksTest, FindsSingleSliderBlockers) {
    const Bitboard rook_snipers = bb::set(E7, E8);

    EXPECT_EQ(attacks::slider_blockers(E1, 0, rook_snipers, bb::set(E1, E7, E8)), 0);
    EXPECT_EQ(attacks::slider_blockers(E1, 0, rook_snipers, bb::set(E1, E2, E7, E8)), bb::set(E2));
    EXPECT_EQ(attacks::slider_blockers(E1, 0, rook_snipers, bb::set(E1, E2, E3, E7, E8)), 0);
    EXPECT_EQ(attacks::slider_blockers(A1, bb::set(H8), 0, bb::set(A1, D4, H8)), bb::set(D4));
}
