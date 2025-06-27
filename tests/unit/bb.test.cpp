#include "bb.hpp"

#include <gtest/gtest.h>

#include <numeric>

#include "test_utils.hpp"
#include "types.hpp"

TEST(BB, CorrectSet) {
    for (int i = 0; i < 64; ++i) {
        U64 result   = BB::set(Square(i));
        U64 expected = 1ULL << i;
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST(BB, CorrectClear) {
    for (int i = 0; i < 64; ++i) {
        U64 result   = BB::clear(Square(i));
        U64 expected = ~(1ULL << i);
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST(BB, CorrectCollinear) {
    EXPECT_EQ(BB::collinear(B2, D2), BB::rankBB(Rank::R2));
    EXPECT_EQ(BB::collinear(D2, B2), BB::rankBB(Rank::R2));
    EXPECT_EQ(BB::collinear(B2, B4), BB::fileBB(File::F2));
    EXPECT_EQ(BB::collinear(B4, B2), BB::fileBB(File::F2));
    EXPECT_EQ(BB::collinear(A1, H8), BB::set(A1, B2, C3, D4, E5, F6, G7, H8));
    EXPECT_EQ(BB::collinear(H8, A1), BB::set(A1, B2, C3, D4, E5, F6, G7, H8));
    EXPECT_EQ(BB::collinear(B2, C4), 0);
    EXPECT_EQ(BB::collinear(C4, B2), 0);
}

TEST(BB, CorrectBetween) {
    EXPECT_EQ(BB::between(B2, D2), BB::set(C2));
    EXPECT_EQ(BB::between(D2, B2), BB::set(C2));
    EXPECT_EQ(BB::between(B2, B4), BB::set(B3));
    EXPECT_EQ(BB::between(B4, B2), BB::set(B3));
    EXPECT_EQ(BB::between(B2, C4), 0);
    EXPECT_EQ(BB::between(C4, B2), 0);
}

TEST(BB, CorrectKnightAttacks) {
    EXPECT_EQ(BB::KNIGHT_ATTACKS[A1], BB::set(B3, C2));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[H1], BB::set(G3, F2));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[A8], BB::set(B6, C7));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[H8], BB::set(G6, F7));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[G2], BB::set(E1, E3, F4, H4));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[C6], BB::set(A5, A7, B4, B8, D4, D8, E5, E7));
}

TEST(BB, CorrectKingAttacks) {
    EXPECT_EQ(BB::KING_ATTACKS[A1], BB::set(A2, B2, B1));
    EXPECT_EQ(BB::KING_ATTACKS[H1], BB::set(H2, G2, G1));
    EXPECT_EQ(BB::KING_ATTACKS[A8], BB::set(A7, B7, B8));
    EXPECT_EQ(BB::KING_ATTACKS[H8], BB::set(H7, G7, G8));
    EXPECT_EQ(BB::KING_ATTACKS[G2], BB::set(F1, F2, F3, G1, G3, H1, H2, H3));
}

TEST(BB, CorrectDistanceValues) {
    EXPECT_EQ(BB::distance(A1, A1), 0);
    EXPECT_EQ(BB::distance(A1, A2), 1);
    EXPECT_EQ(BB::distance(A1, B1), 1);
    EXPECT_EQ(BB::distance(A1, B2), 1);
    EXPECT_EQ(BB::distance(A1, G7), 6);
    EXPECT_EQ(BB::distance(A1, H7), 7);
    EXPECT_EQ(BB::distance(A1, G8), 7);
    EXPECT_EQ(BB::distance(A1, H8), 7);
}

TEST(BB, CorrectMoreThanOneValues) {
    EXPECT_EQ(BB::isMany(0b100), 0);
    EXPECT_NE(BB::isMany(0b110), 0);
}

TEST(BB, CorrectBitCount) {
    EXPECT_EQ(BB::count(0b0), 0);
    EXPECT_EQ(BB::count(0b1000), 1);
    EXPECT_EQ(BB::count(0b1010), 2);
    EXPECT_EQ(BB::count(0b1011), 3);
}

TEST(BB, CorrectLeastSignificantBit) {
    EXPECT_EQ(BB::lsb(BB::set(A1)), A1);
    EXPECT_EQ(BB::lsb(BB::set(H1)), H1);
    EXPECT_EQ(BB::lsb(BB::set(A8)), A8);
    EXPECT_EQ(BB::lsb(BB::set(H8)), H8);
}

TEST(BB, CorrectMostSignificantBit) {
    EXPECT_EQ(BB::msb(BB::set(A1)), A1);
    EXPECT_EQ(BB::msb(BB::set(H1)), H1);
    EXPECT_EQ(BB::msb(BB::set(A8)), A8);
    EXPECT_EQ(BB::msb(BB::set(H8)), H8);
}

TEST(BB, CorrectLeastSignificantBitPop) {
    U64 bb = BB::set(A1, B2, C3);
    EXPECT_EQ(BB::lsbPop(bb), A1);
    EXPECT_EQ(BB::lsbPop(bb), B2);
    EXPECT_EQ(BB::lsbPop(bb), C3);
    EXPECT_EQ(bb, 0);
}

TEST(BB, CorrectMostSignificantBitPop) {
    U64 bb = BB::set(A1, B2, C3);
    EXPECT_EQ(BB::msbPop(bb), C3);
    EXPECT_EQ(BB::msbPop(bb), B2);
    EXPECT_EQ(BB::msbPop(bb), A1);
    EXPECT_EQ(bb, 0);
}

TEST(BB, CorrectFillNorthValues) {
    EXPECT_EQ(BB::fillNorth(BB::set(A1)), BB::fileBB(File::F1));
    EXPECT_EQ(BB::fillNorth(BB::set(H1)), BB::fileBB(File::F8));
    EXPECT_EQ(BB::fillNorth(BB::set(A7)), BB::set(A7, A8));
    EXPECT_EQ(BB::fillNorth(BB::set(H7)), BB::set(H7, H8));
}

TEST(BB, CorrectFillSouthValues) {
    EXPECT_EQ(BB::fillSouth(BB::set(A8)), BB::fileBB(File::F1));
    EXPECT_EQ(BB::fillSouth(BB::set(H8)), BB::fileBB(File::F8));
    EXPECT_EQ(BB::fillSouth(BB::set(A2)), BB::set(A2, A1));
    EXPECT_EQ(BB::fillSouth(BB::set(H2)), BB::set(H2, H1));
}
