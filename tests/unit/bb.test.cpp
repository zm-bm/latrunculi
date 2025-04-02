#include "bb.hpp"

#include <gtest/gtest.h>

#include <numeric>

#include "types.hpp"

TEST(BB_set, CorrectSetBit) {
    for (int i = 0; i < 64; ++i) {
        U64 result   = BB::set(Square(i));
        U64 expected = 1ULL << i;
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST(BB_clear, CorrectClearBit) {
    for (int i = 0; i < 64; ++i) {
        U64 result   = BB::clear(Square(i));
        U64 expected = ~(1ULL << i);
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

U64 targets(const std::vector<Square>& squares) {
    return std::accumulate(squares.begin(), squares.end(), 0ULL, [](U64 acc, Square sq) {
        return acc | BB::set(sq);
    });
}

TEST(BB_inline, CorrectBitsInlineValues) {
    EXPECT_EQ(BB::inlineBB(B2, D2), BB::RANK_MASK[RANK2]);
    EXPECT_EQ(BB::inlineBB(D2, B2), BB::RANK_MASK[RANK2]);
    EXPECT_EQ(BB::inlineBB(B2, B4), BB::FILE_MASK[FILE2]);
    EXPECT_EQ(BB::inlineBB(B4, B2), BB::FILE_MASK[FILE2]);
    EXPECT_EQ(BB::inlineBB(A1, H8), targets({A1, B2, C3, D4, E5, F6, G7, H8}));
    EXPECT_EQ(BB::inlineBB(H8, A1), targets({A1, B2, C3, D4, E5, F6, G7, H8}));
    EXPECT_EQ(BB::inlineBB(B2, C4), 0);
    EXPECT_EQ(BB::inlineBB(C4, B2), 0);
}

TEST(BB_bitsBtwn, CorrectBitsBtwnValues) {
    EXPECT_EQ(BB::betweenBB(B2, D2), BB::set(C2));
    EXPECT_EQ(BB::betweenBB(D2, B2), BB::set(C2));
    EXPECT_EQ(BB::betweenBB(B2, B4), BB::set(B3));
    EXPECT_EQ(BB::betweenBB(B4, B2), BB::set(B3));
    EXPECT_EQ(BB::betweenBB(B2, C4), 0);
    EXPECT_EQ(BB::betweenBB(C4, B2), 0);
}

TEST(BB_KNIGHT_ATTACKS, CorrectKnightAttacks) {
    EXPECT_EQ(BB::KNIGHT_ATTACKS[A1], targets({B3, C2}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[H1], targets({G3, F2}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[A8], targets({B6, C7}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[H8], targets({G6, F7}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[G2], targets({E1, E3, F4, H4}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[C6], targets({A5, A7, B4, B8, D4, D8, E5, E7}));
}

TEST(BB_KING_ATTACKS, CorrectKingAttacks) {
    EXPECT_EQ(BB::KING_ATTACKS[A1], targets({A2, B2, B1}));
    EXPECT_EQ(BB::KING_ATTACKS[H1], targets({H2, G2, G1}));
    EXPECT_EQ(BB::KING_ATTACKS[A8], targets({A7, B7, B8}));
    EXPECT_EQ(BB::KING_ATTACKS[H8], targets({H7, G7, G8}));
    EXPECT_EQ(BB::KING_ATTACKS[G2], targets({F1, F2, F3, G1, G3, H1, H2, H3}));
}

TEST(BB_DISTANCE, CorrectDistanceValues) {
    EXPECT_EQ(BB::DISTANCE[A1][A1], 0);
    EXPECT_EQ(BB::DISTANCE[A1][A2], 1);
    EXPECT_EQ(BB::DISTANCE[A1][B1], 1);
    EXPECT_EQ(BB::DISTANCE[A1][B2], 1);
    EXPECT_EQ(BB::DISTANCE[A1][G7], 6);
    EXPECT_EQ(BB::DISTANCE[A1][H7], 7);
    EXPECT_EQ(BB::DISTANCE[A1][G8], 7);
    EXPECT_EQ(BB::DISTANCE[A1][H8], 7);
}

TEST(BB_hasMoreThanOne, CorrectMoreThanOneValues) {
    EXPECT_EQ(BB::hasMoreThanOne(0b100), 0);
    EXPECT_NE(BB::hasMoreThanOne(0b110), 0);
}

TEST(BB_bitCount, CorrectBitCount) {
    EXPECT_EQ(BB::count(0b0), 0);
    EXPECT_EQ(BB::count(0b1000), 1);
    EXPECT_EQ(BB::count(0b1010), 2);
    EXPECT_EQ(BB::count(0b1011), 3);
}

TEST(BB_lsb, CorrectLeastSignificantBit) {
    EXPECT_EQ(BB::lsb(BB::set(A1)), A1);
    EXPECT_EQ(BB::lsb(BB::set(H1)), H1);
    EXPECT_EQ(BB::lsb(BB::set(A8)), A8);
    EXPECT_EQ(BB::lsb(BB::set(H8)), H8);
}

TEST(BB_msb, CorrectMostSignificantBit) {
    EXPECT_EQ(BB::msb(BB::set(A1)), A1);
    EXPECT_EQ(BB::msb(BB::set(H1)), H1);
    EXPECT_EQ(BB::msb(BB::set(A8)), A8);
    EXPECT_EQ(BB::msb(BB::set(H8)), H8);
}

TEST(BB_fillNorth, CorrectFillValues) {
    EXPECT_EQ(BB::fillNorth(BB::set(A1)), BB::FILE_MASK[FILE1]);
    EXPECT_EQ(BB::fillNorth(BB::set(H1)), BB::FILE_MASK[FILE8]);
    EXPECT_EQ(BB::fillNorth(BB::set(A7)), BB::set(A7) | BB::set(A8));
    EXPECT_EQ(BB::fillNorth(BB::set(H7)), BB::set(H7) | BB::set(H8));
}

TEST(BB_fillSouth, CorrectFillValues) {
    EXPECT_EQ(BB::fillSouth(BB::set(A8)), BB::FILE_MASK[FILE1]);
    EXPECT_EQ(BB::fillSouth(BB::set(H8)), BB::FILE_MASK[FILE8]);
    EXPECT_EQ(BB::fillSouth(BB::set(A2)), BB::set(A2) | BB::set(A1));
    EXPECT_EQ(BB::fillSouth(BB::set(H2)), BB::set(H2) | BB::set(H1));
}
