#include "defs.hpp"

#include <gtest/gtest.h>

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
