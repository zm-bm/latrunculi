#include <gtest/gtest.h>
#include "globals.hpp"

class GlobalsTest : public ::testing::Test {
protected:
    void SetUp() override {
        G::init();
    }
};

TEST_F(GlobalsTest, bitset) {
    for (int i = 0; i < 64; ++i) {
        U64 result = G::bitset(static_cast<Square>(i));
        U64 expected = 1ULL << i;
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST_F(GlobalsTest, bitclear) {
    for (int i = 0; i < 64; ++i) {
        U64 result = G::bitclear(static_cast<Square>(i));
        U64 expected = ~(1ULL << i);
        ASSERT_EQ(result, expected) << "Failed at index " << i;
    }
}

TEST_F(GlobalsTest, knightAttacks) {
    U64 expected = (G::bitset(B3) | G::bitset(C2));
    EXPECT_EQ(G::KNIGHT_ATTACKS[A1], expected);

    expected = (
        G::bitset(E1)
        | G::bitset(E3)
        | G::bitset(F4)
        | G::bitset(H4)
    );
    EXPECT_EQ(G::KNIGHT_ATTACKS[G2], expected);

    expected = (
        G::bitset(A5)
        | G::bitset(A7)
        | G::bitset(B4)
        | G::bitset(B8)
        | G::bitset(D4)
        | G::bitset(D8)
        | G::bitset(E5)
        | G::bitset(E7)
    );
    EXPECT_EQ(G::KNIGHT_ATTACKS[C6], expected);
}

TEST_F(GlobalsTest, kingAttacks) {
    U64 expected = (
        G::bitset(A2)
        | G::bitset(B2)
        | G::bitset(B1)
    );
    EXPECT_EQ(G::KING_ATTACKS[A1], expected);

    expected = (
        G::bitset(F1)
        | G::bitset(F2)
        | G::bitset(F3)
        | G::bitset(G1)
        | G::bitset(G3)
        | G::bitset(H1)
        | G::bitset(H2)
        | G::bitset(H3)
    );
    EXPECT_EQ(G::KING_ATTACKS[G2], expected);
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
