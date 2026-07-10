#include "core/attacks.hpp"

#include <gtest/gtest.h>

TEST(AttacksTest, CorrectPawnMoves) {
    uint64_t pawns = bb::set(D4);
    EXPECT_EQ(attacks::pawn_moves<PAWN_PUSH>(pawns, WHITE), bb::set(D5));
    EXPECT_EQ(attacks::pawn_moves<PAWN_PUSH>(pawns, BLACK), bb::set(D3));
    EXPECT_EQ(attacks::pawn_moves<PAWN_LEFT>(pawns, WHITE), bb::set(C5));
    EXPECT_EQ(attacks::pawn_moves<PAWN_RIGHT>(pawns, WHITE), bb::set(E5));
    EXPECT_EQ(attacks::pawn_moves<PAWN_LEFT>(pawns, BLACK), bb::set(E3));
    EXPECT_EQ(attacks::pawn_moves<PAWN_RIGHT>(pawns, BLACK), bb::set(C3));
    EXPECT_EQ(attacks::pawn_moves<PAWN_PUSH2>(pawns, WHITE), bb::set(D6));
    EXPECT_EQ(attacks::pawn_moves<PAWN_PUSH2>(pawns, BLACK), bb::set(D2));

    uint64_t pawns_left = bb::set(A4);
    EXPECT_EQ(attacks::pawn_moves<PAWN_LEFT>(pawns_left, WHITE), 0);
    EXPECT_EQ(attacks::pawn_moves<PAWN_RIGHT>(pawns_left, BLACK), 0);

    uint64_t pawns_right = bb::set(H4);
    EXPECT_EQ(attacks::pawn_moves<PAWN_RIGHT>(pawns_right, WHITE), 0);
    EXPECT_EQ(attacks::pawn_moves<PAWN_LEFT>(pawns_right, BLACK), 0);
}

TEST(AttacksTest, CorrectPawnAttacks) {
    uint64_t pawns = bb::set(A4, D4, H4);
    EXPECT_EQ(attacks::pawn_attacks(pawns, WHITE), bb::set(B5, C5, E5, G5));
    EXPECT_EQ(attacks::pawn_attacks(pawns, BLACK), bb::set(B3, C3, E3, G3));
}

TEST(AttacksTest, CorrectMovesKnights) {
    EXPECT_EQ(attacks::piece_moves<KNIGHT>(A1), bb::set(B3, C2));
    EXPECT_EQ(attacks::piece_moves<KNIGHT>(H1), bb::set(G3, F2));
    EXPECT_EQ(attacks::piece_moves<KNIGHT>(A8), bb::set(B6, C7));
    EXPECT_EQ(attacks::piece_moves<KNIGHT>(H8), bb::set(G6, F7));
    EXPECT_EQ(attacks::piece_moves<KNIGHT>(G2), bb::set(E1, E3, F4, H4));
    EXPECT_EQ(attacks::piece_moves<KNIGHT>(C6), bb::set(A5, A7, B4, B8, D4, D8, E5, E7));
}

TEST(AttacksTest, CorrectMovesKings) {
    EXPECT_EQ(attacks::piece_moves<KING>(A1), bb::set(A2, B2, B1));
    EXPECT_EQ(attacks::piece_moves<KING>(H1), bb::set(H2, G2, G1));
    EXPECT_EQ(attacks::piece_moves<KING>(A8), bb::set(A7, B7, B8));
    EXPECT_EQ(attacks::piece_moves<KING>(H8), bb::set(H7, G7, G8));
    EXPECT_EQ(attacks::piece_moves<KING>(G2), bb::set(F1, F2, F3, G1, G3, H1, H2, H3));
}
