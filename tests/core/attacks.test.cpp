#include "core/attacks.hpp"

#include <gtest/gtest.h>

TEST(AttacksTest, CorrectPawnMoves) {
    Bitboard pawns = bb::set(D4);
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::push>(pawns, WHITE), bb::set(D5));
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::push>(pawns, BLACK), bb::set(D3));
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::left>(pawns, WHITE), bb::set(C5));
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::right>(pawns, WHITE), bb::set(E5));
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::left>(pawns, BLACK), bb::set(E3));
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::right>(pawns, BLACK), bb::set(C3));
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::double_push>(pawns, WHITE), bb::set(D6));
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::double_push>(pawns, BLACK), bb::set(D2));

    Bitboard pawns_left = bb::set(A4);
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::left>(pawns_left, WHITE), 0);
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::right>(pawns_left, BLACK), 0);

    Bitboard pawns_right = bb::set(H4);
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::right>(pawns_right, WHITE), 0);
    EXPECT_EQ(attacks::pawn_moves<pawn_delta::left>(pawns_right, BLACK), 0);
}

TEST(AttacksTest, CorrectPawnAttacks) {
    Bitboard pawns = bb::set(A4, D4, H4);
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
