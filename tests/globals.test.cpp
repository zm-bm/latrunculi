#include "globals.hpp"

#include <gtest/gtest.h>

TEST(GlobalsTest, BITSET) {
    for (int i = 0; i < 64; ++i) {
        U64 result = G::BITSET[i];
        U64 expected = 1ULL << i;
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST(GlobalsTest, BITCLEAR) {
    for (int i = 0; i < 64; ++i) {
        U64 result = G::BITCLEAR[i];
        U64 expected = ~(1ULL << i);
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

U64 targetBitboard(std::vector<Square> squares) {
    U64 target = 0ULL;
    for (auto sq : squares) {
        target |= G::BITSET[sq];
    }
    return target;
}

TEST(GlobalsTest, KNIGHT_ATTACKS) {
    EXPECT_EQ(G::KNIGHT_ATTACKS[A1], targetBitboard({B3, C2, C3}));
    EXPECT_EQ(G::KNIGHT_ATTACKS[H1], targetBitboard({G3, F2}));
    EXPECT_EQ(G::KNIGHT_ATTACKS[A8], targetBitboard({B6, C7}));
    EXPECT_EQ(G::KNIGHT_ATTACKS[H8], targetBitboard({G6, F7}));
    EXPECT_EQ(G::KNIGHT_ATTACKS[G2], targetBitboard({E1, E3, F4, H4}));
    EXPECT_EQ(G::KNIGHT_ATTACKS[C6],
              targetBitboard({A5, A7, B4, B8, D4, D8, E5, E7}));
}

TEST(GlobalsTest, KING_ATTACKS) {
    EXPECT_EQ(G::KING_ATTACKS[A1], targetBitboard({A2, B2, B1}));
    EXPECT_EQ(G::KING_ATTACKS[H1], targetBitboard({H2, G2, G1}));
    EXPECT_EQ(G::KING_ATTACKS[A8], targetBitboard({A7, B7, B8}));
    EXPECT_EQ(G::KING_ATTACKS[H8], targetBitboard({H7, G7, G8}));
    EXPECT_EQ(G::KING_ATTACKS[G2],
              targetBitboard({F1, F2, F3, G1, G3, H1, H2, H3}));
}

TEST(GlobalsTest, DISTANCE) {
    EXPECT_EQ(G::DISTANCE[A1][A1], 0);
    EXPECT_EQ(G::DISTANCE[A1][A2], 1);
    EXPECT_EQ(G::DISTANCE[A1][B1], 1);
    EXPECT_EQ(G::DISTANCE[A1][B2], 1);
    EXPECT_EQ(G::DISTANCE[A1][G7], 6);
    EXPECT_EQ(G::DISTANCE[A1][H7], 7);
    EXPECT_EQ(G::DISTANCE[A1][G8], 7);
    EXPECT_EQ(G::DISTANCE[A1][H8], 7);
}

TEST(GlobalsTest, BITS_BETWEEN) {
    EXPECT_EQ(G::BITS_BETWEEN[A1][A1], 0);
    EXPECT_EQ(G::BITS_BETWEEN[A1][A2], 0);
    EXPECT_EQ(G::BITS_BETWEEN[A1][A3], targetBitboard({A2}));
    EXPECT_EQ(G::BITS_BETWEEN[A1][A8],
              targetBitboard({A2, A3, A4, A5, A6, A7}));
    EXPECT_EQ(G::BITS_BETWEEN[A1][B1], 0);
    EXPECT_EQ(G::BITS_BETWEEN[A1][B2], 0);
    EXPECT_EQ(G::BITS_BETWEEN[A1][B3], 0);
    EXPECT_EQ(G::BITS_BETWEEN[A1][C1], targetBitboard({B1}));
    EXPECT_EQ(G::BITS_BETWEEN[A1][C2], 0);
    EXPECT_EQ(G::BITS_BETWEEN[A1][C3], targetBitboard({B2}));
    EXPECT_EQ(G::BITS_BETWEEN[A1][H1],
              targetBitboard({B1, C1, D1, E1, F1, G1}));
    EXPECT_EQ(G::BITS_BETWEEN[A1][H8],
              targetBitboard({B2, C3, D4, E5, F6, G7}));
}

TEST(GlobalsTest, BITS_INLINE) {
    EXPECT_EQ(G::BITS_INLINE[A1][A2],
              targetBitboard({A1, A2, A3, A4, A5, A6, A7, A8}));
    EXPECT_EQ(G::BITS_INLINE[A1][A8],
              targetBitboard({A1, A2, A3, A4, A5, A6, A7, A8}));
    EXPECT_EQ(G::BITS_INLINE[A1][B1],
              targetBitboard({A1, B1, C1, D1, E1, F1, G1, H1}));
    EXPECT_EQ(G::BITS_INLINE[A1][H1],
              targetBitboard({A1, B1, C1, D1, E1, F1, G1, H1}));
    EXPECT_EQ(G::BITS_INLINE[A1][B2],
              targetBitboard({A1, B2, C3, D4, E5, F6, G7, H8}));
    EXPECT_EQ(G::BITS_INLINE[A1][H8],
              targetBitboard({A1, B2, C3, D4, E5, F6, G7, H8}));
}

TEST(SplitTest, split) {
    std::string input = "a,b,c";
    char delimiter = ',';
    std::vector<std::string> expected = {"a", "b", "c"};

    EXPECT_EQ(G::split(input, delimiter), expected);
}

TEST(SplitTest, splitWithConsecutiveDelimiters) {
    std::string input = "a,,b,c";
    char delimiter = ',';
    std::vector<std::string> expected = {"a", "", "b", "c"};

    EXPECT_EQ(G::split(input, delimiter), expected);
}

TEST(SplitTest, splitWithoutDelimiter) {
    std::string input = "abc";
    char delimiter = ',';
    std::vector<std::string> expected = {"abc"};

    EXPECT_EQ(G::split(input, delimiter), expected);
}

TEST(SplitTest, splitEmptyString) {
    std::string input = "";
    char delimiter = ',';
    std::vector<std::string> expected = {};

    EXPECT_EQ(G::split(input, delimiter), expected);
}

TEST(SplitTest, splitWithSpecialCharacters) {
    std::string input = "a!b@c#d$";
    char delimiter = '@';
    std::vector<std::string> expected = {"a!b", "c#d$"};

    EXPECT_EQ(G::split(input, delimiter), expected);
}
