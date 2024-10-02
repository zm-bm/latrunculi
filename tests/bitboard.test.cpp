#include "bb.hpp"

#include <gtest/gtest.h>

#include "types.hpp"

TEST(BitboardTest, Set) {
    U64 expected = 1ULL << 9;
    EXPECT_EQ(BB::set(B2), expected);
}

TEST(BitboardTest, Clear) {
    U64 expected = ~(1ULL << 9);
    EXPECT_EQ(BB::clear(B2), expected);
}

TEST(BitboardTest, BitsInline) {
    EXPECT_EQ(BB::bitsInline(B2, D2), G::RANK_MASK[RANK2]);
    EXPECT_EQ(BB::bitsInline(D2, B2), G::RANK_MASK[RANK2]);

    EXPECT_EQ(BB::bitsInline(B2, B4), G::FILE_MASK[FILE2]);
    EXPECT_EQ(BB::bitsInline(B4, B2), G::FILE_MASK[FILE2]);
    
    EXPECT_EQ(BB::bitsInline(B2, C4), 0);
    EXPECT_EQ(BB::bitsInline(C4, B2), 0);
}

TEST(BitboardTest, BitsBtwn) {
    EXPECT_EQ(BB::bitsBtwn(B2, D2), BB::set(C2));
    EXPECT_EQ(BB::bitsBtwn(D2, B2), BB::set(C2));

    EXPECT_EQ(BB::bitsBtwn(B2, B4), BB::set(B3));
    EXPECT_EQ(BB::bitsBtwn(B4, B2), BB::set(B3));
    
    EXPECT_EQ(BB::bitsBtwn(B2, C4), 0);
    EXPECT_EQ(BB::bitsBtwn(C4, B2), 0);
}

TEST(BitboardTest, MoreThanOneSet) {
    EXPECT_EQ(BB::moreThanOneSet(0b100), 0);
    EXPECT_NE(BB::moreThanOneSet(0b110), 0);
}

TEST(BitboardTest, BitCount) {
    EXPECT_EQ(BB::bitCount(0b0), 0);
    EXPECT_EQ(BB::bitCount(0b1000), 1);
    EXPECT_EQ(BB::bitCount(0b1010), 2);
    EXPECT_EQ(BB::bitCount(0b1011), 3);
}

TEST(BitboardTest, Lsb) {
    EXPECT_EQ(BB::lsb(BB::set(A1)), A1);
    EXPECT_EQ(BB::lsb(BB::set(H1)), H1);
    EXPECT_EQ(BB::lsb(BB::set(A8)), A8);
    EXPECT_EQ(BB::lsb(BB::set(H8)), H8);
}

TEST(BitboardTest, Msb) {
    EXPECT_EQ(BB::msb(BB::set(A1)), A1);
    EXPECT_EQ(BB::msb(BB::set(H1)), H1);
    EXPECT_EQ(BB::msb(BB::set(A8)), A8);
    EXPECT_EQ(BB::msb(BB::set(H8)), H8);
}

TEST(BitboardTest, FillNorth) {
    EXPECT_EQ(BB::fillNorth(BB::set(A1)), G::FILE_MASK[FILE1]);
    EXPECT_EQ(BB::fillNorth(BB::set(H1)), G::FILE_MASK[FILE8]);
    EXPECT_EQ(BB::fillNorth(BB::set(A7)), BB::set(A7) | BB::set(A8));
    EXPECT_EQ(BB::fillNorth(BB::set(H7)), BB::set(H7) | BB::set(H8));
}

TEST(BitboardTest, FillSouth) {
    EXPECT_EQ(BB::fillSouth(BB::set(A8)), G::FILE_MASK[FILE1]);
    EXPECT_EQ(BB::fillSouth(BB::set(H8)), G::FILE_MASK[FILE8]);
    EXPECT_EQ(BB::fillSouth(BB::set(A2)), BB::set(A2) | BB::set(A1));
    EXPECT_EQ(BB::fillSouth(BB::set(H2)), BB::set(H2) | BB::set(H1));
}
