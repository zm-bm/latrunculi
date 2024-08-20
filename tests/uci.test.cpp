#include <gtest/gtest.h>
#include "uci.hpp"

// A simple function to demonstrate testing
int add(int a, int b) {
    return a + b;
}

// Test case for the add function
TEST(AdditionTest, HandlesPositiveNumbers) {
    EXPECT_EQ(add(2, 3), 5);
    EXPECT_EQ(add(10, 20), 30);
}

// Test case for negative numbers
TEST(AdditionTest, HandlesNegativeNumbers) {
    EXPECT_EQ(add(-2, -3), -5);
    EXPECT_EQ(add(-10, -20), -30);
}

// Test case for mixed positive and negative numbers
TEST(AdditionTest, HandlesMixedNumbers) {
    EXPECT_EQ(add(2, -3), -1);
    EXPECT_EQ(add(-10, 20), 10);
}

// Test case for zeros
TEST(AdditionTest, HandlesZero) {
    EXPECT_EQ(add(0, 0), 0);
    EXPECT_EQ(add(0, 5), 5);
    EXPECT_EQ(add(-5, 0), -5);
}
