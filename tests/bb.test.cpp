#include "bb.hpp"

#include <gtest/gtest.h>

#include "types.hpp"

TEST(BitboardTest, Set) {
    U64 expected = 1ULL << 9;
    EXPECT_EQ(BB::set(B2), expected) << "should set a single bit";
}

TEST(BitboardTest, Clear) {
    U64 expected = ~(1ULL << 9);
    EXPECT_EQ(BB::clear(B2), expected) << "should unset a single bit";
}

TEST(BitboardTest, BitsInline) {
    EXPECT_EQ(BB::bitsInline(B2, D2), G::RANK_MASK[RANK2])
        << "should set rank bits when squares in rank";
    EXPECT_EQ(BB::bitsInline(D2, B2), G::RANK_MASK[RANK2])
        << "should set rank bits when squares in rank";
    EXPECT_EQ(BB::bitsInline(B2, B4), G::FILE_MASK[FILE2])
        << "should set file bits when squares in file";
    EXPECT_EQ(BB::bitsInline(B4, B2), G::FILE_MASK[FILE2])
        << "should set file bits when squares in file";
    EXPECT_EQ(BB::bitsInline(A1, H8), 0x8040201008040201)
        << "should set bits when squares diagonal";
    EXPECT_EQ(BB::bitsInline(H8, A1), 0x8040201008040201)
        << "should set bits when squares diagonal";
    EXPECT_EQ(BB::bitsInline(B2, C4), 0)
        << "should be zero when squares not in line";
    EXPECT_EQ(BB::bitsInline(C4, B2), 0)
        << "should be zero when squares not in line";
}

TEST(BitboardTest, BitsBtwn) {
    EXPECT_EQ(BB::bitsBtwn(B2, D2), BB::set(C2))
        << "should set bits between when squares in rank";
    EXPECT_EQ(BB::bitsBtwn(D2, B2), BB::set(C2))
        << "should set bits between when squares in rank";
    EXPECT_EQ(BB::bitsBtwn(B2, B4), BB::set(B3))
        << "should set bits between when squares in file";
    EXPECT_EQ(BB::bitsBtwn(B4, B2), BB::set(B3))
        << "should set bits between when squares in file";
    EXPECT_EQ(BB::bitsBtwn(B2, C4), 0)
        << "should be zero when squares not in line";
    EXPECT_EQ(BB::bitsBtwn(C4, B2), 0)
        << "should be zero when squares not in line";
}

TEST(BitboardTest, MoreThanOneSet) {
    EXPECT_EQ(BB::moreThanOneSet(0b100), 0)
        << "should be zero when one bit is set";
    EXPECT_NE(BB::moreThanOneSet(0b110), 0)
        << "should be one when two bits are set";
}

TEST(BitboardTest, BitCount) {
    EXPECT_EQ(BB::bitCount(0b0), 0) << "should count bits";
    EXPECT_EQ(BB::bitCount(0b1000), 1) << "should count bits";
    EXPECT_EQ(BB::bitCount(0b1010), 2) << "should count bits";
    EXPECT_EQ(BB::bitCount(0b1011), 3) << "should count bits";
}

TEST(BitboardTest, Lsb) {
    EXPECT_EQ(BB::lsb(BB::set(A1)), A1) << "should return least sig bit";
    EXPECT_EQ(BB::lsb(BB::set(H1)), H1) << "should return least sig bit";
    EXPECT_EQ(BB::lsb(BB::set(A8)), A8) << "should return least sig bit";
    EXPECT_EQ(BB::lsb(BB::set(H8)), H8) << "should return least sig bit";
}

TEST(BitboardTest, Msb) {
    EXPECT_EQ(BB::msb(BB::set(A1)), A1) << "should return most sig bit";
    EXPECT_EQ(BB::msb(BB::set(H1)), H1) << "should return most sig bit";
    EXPECT_EQ(BB::msb(BB::set(A8)), A8) << "should return most sig bit";
    EXPECT_EQ(BB::msb(BB::set(H8)), H8) << "should return most sig bit";
}

TEST(BitboardTest, FillNorth) {
    EXPECT_EQ(BB::fillNorth(BB::set(A1)), G::FILE_MASK[FILE1])
        << "should fill bits";
    EXPECT_EQ(BB::fillNorth(BB::set(H1)), G::FILE_MASK[FILE8])
        << "should fill bits";
    EXPECT_EQ(BB::fillNorth(BB::set(A7)), BB::set(A7) | BB::set(A8))
        << "should fill bits";
    EXPECT_EQ(BB::fillNorth(BB::set(H7)), BB::set(H7) | BB::set(H8))
        << "should fill bits";
}

TEST(BitboardTest, FillSouth) {
    EXPECT_EQ(BB::fillSouth(BB::set(A8)), G::FILE_MASK[FILE1])
        << "should fill bits";
    EXPECT_EQ(BB::fillSouth(BB::set(H8)), G::FILE_MASK[FILE8])
        << "should fill bits";
    EXPECT_EQ(BB::fillSouth(BB::set(A2)), BB::set(A2) | BB::set(A1))
        << "should fill bits";
    EXPECT_EQ(BB::fillSouth(BB::set(H2)), BB::set(H2) | BB::set(H1))
        << "should fill bits";
}
