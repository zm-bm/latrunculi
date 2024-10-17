#include "bb.hpp"

#include <gtest/gtest.h>

#include "types.hpp"

TEST(BitboardTest, Set) {
    for (int i = 0; i < 64; ++i) {
        U64 result = BB::BITSET[i];
        U64 expected = 1ULL << i;
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST(BitboardTest, Clear) {
    for (int i = 0; i < 64; ++i) {
        U64 result = BB::BITCLEAR[i];
        U64 expected = ~(1ULL << i);
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

U64 targets(std::vector<Square> squares) {
    U64 target = 0ULL;
    for (auto sq : squares) {
        target |= BB::BITSET[sq];
    }
    return target;
}

TEST(BitboardTest, BitsInline) {
    EXPECT_EQ(BB::bitsInline(B2, D2), BB::RANK_MASK[RANK2]) << "should set rank bits when squares in rank";
    EXPECT_EQ(BB::bitsInline(D2, B2), BB::RANK_MASK[RANK2]) << "should set rank bits when squares in rank";
    EXPECT_EQ(BB::bitsInline(B2, B4), BB::FILE_MASK[FILE2]) << "should set file bits when squares in file";
    EXPECT_EQ(BB::bitsInline(B4, B2), BB::FILE_MASK[FILE2]) << "should set file bits when squares in file";
    EXPECT_EQ(BB::bitsInline(A1, H8), targets({A1, B2, C3, D4, E5, F6, G7, H8}))
        << "should set bits when squares diagonal";
    EXPECT_EQ(BB::bitsInline(H8, A1), targets({A1, B2, C3, D4, E5, F6, G7, H8}))
        << "should set bits when squares diagonal";
    EXPECT_EQ(BB::bitsInline(B2, C4), 0) << "should be zero when squares not in line";
    EXPECT_EQ(BB::bitsInline(C4, B2), 0) << "should be zero when squares not in line";
}

TEST(BitboardTest, BitsBtwn) {
    EXPECT_EQ(BB::bitsBtwn(B2, D2), BB::set(C2)) << "should set bits between when squares in rank";
    EXPECT_EQ(BB::bitsBtwn(D2, B2), BB::set(C2)) << "should set bits between when squares in rank";
    EXPECT_EQ(BB::bitsBtwn(B2, B4), BB::set(B3)) << "should set bits between when squares in file";
    EXPECT_EQ(BB::bitsBtwn(B4, B2), BB::set(B3)) << "should set bits between when squares in file";
    EXPECT_EQ(BB::bitsBtwn(B2, C4), 0) << "should be zero when squares not in line";
    EXPECT_EQ(BB::bitsBtwn(C4, B2), 0) << "should be zero when squares not in line";
}

TEST(BitboardTest, KNIGHT_ATTACKS) {
    EXPECT_EQ(BB::KNIGHT_ATTACKS[A1], targets({B3, C2}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[H1], targets({G3, F2}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[A8], targets({B6, C7}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[H8], targets({G6, F7}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[G2], targets({E1, E3, F4, H4}));
    EXPECT_EQ(BB::KNIGHT_ATTACKS[C6], targets({A5, A7, B4, B8, D4, D8, E5, E7}));
}

TEST(BitboardTest, KING_ATTACKS) {
    EXPECT_EQ(BB::KING_ATTACKS[A1], targets({A2, B2, B1}));
    EXPECT_EQ(BB::KING_ATTACKS[H1], targets({H2, G2, G1}));
    EXPECT_EQ(BB::KING_ATTACKS[A8], targets({A7, B7, B8}));
    EXPECT_EQ(BB::KING_ATTACKS[H8], targets({H7, G7, G8}));
    EXPECT_EQ(BB::KING_ATTACKS[G2], targets({F1, F2, F3, G1, G3, H1, H2, H3}));
}

TEST(BitboardTest, MoreThanOneSet) {
    EXPECT_EQ(BB::moreThanOneSet(0b100), 0) << "should be zero when one bit is set";
    EXPECT_NE(BB::moreThanOneSet(0b110), 0) << "should be one when two bits are set";
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
    EXPECT_EQ(BB::fillNorth(BB::set(A1)), BB::FILE_MASK[FILE1]) << "should fill bits";
    EXPECT_EQ(BB::fillNorth(BB::set(H1)), BB::FILE_MASK[FILE8]) << "should fill bits";
    EXPECT_EQ(BB::fillNorth(BB::set(A7)), BB::set(A7) | BB::set(A8)) << "should fill bits";
    EXPECT_EQ(BB::fillNorth(BB::set(H7)), BB::set(H7) | BB::set(H8)) << "should fill bits";
}

TEST(BitboardTest, FillSouth) {
    EXPECT_EQ(BB::fillSouth(BB::set(A8)), BB::FILE_MASK[FILE1]) << "should fill bits";
    EXPECT_EQ(BB::fillSouth(BB::set(H8)), BB::FILE_MASK[FILE8]) << "should fill bits";
    EXPECT_EQ(BB::fillSouth(BB::set(A2)), BB::set(A2) | BB::set(A1)) << "should fill bits";
    EXPECT_EQ(BB::fillSouth(BB::set(H2)), BB::set(H2) | BB::set(H1)) << "should fill bits";
}
