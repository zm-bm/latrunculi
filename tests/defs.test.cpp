#include "defs.hpp"

#include <gtest/gtest.h>

TEST(DefsTest, split) {
    std::string input = "a,b,c";
    char delimiter = ',';
    std::vector<std::string> expected = {"a", "b", "c"};

    EXPECT_EQ(LtrnDefs::split(input, delimiter), expected);
}

TEST(DefsTest, splitWithConsecutiveDelimiters) {
    std::string input = "a,,b,c";
    char delimiter = ',';
    std::vector<std::string> expected = {"a", "", "b", "c"};

    EXPECT_EQ(LtrnDefs::split(input, delimiter), expected);
}

TEST(DefsTest, splitWithoutDelimiter) {
    std::string input = "abc";
    char delimiter = ',';
    std::vector<std::string> expected = {"abc"};

    EXPECT_EQ(LtrnDefs::split(input, delimiter), expected);
}

TEST(DefsTest, splitEmptyString) {
    std::string input = "";
    char delimiter = ',';
    std::vector<std::string> expected = {};

    EXPECT_EQ(LtrnDefs::split(input, delimiter), expected);
}

TEST(DefsTest, splitWithSpecialCharacters) {
    std::string input = "a!b@c#d$";
    char delimiter = '@';
    std::vector<std::string> expected = {"a!b", "c#d$"};

    EXPECT_EQ(LtrnDefs::split(input, delimiter), expected);
}
