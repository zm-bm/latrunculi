#include "score.hpp"

#include <gtest/gtest.h>

#include "constants.hpp"

TEST(ScoreTest, Equality) {
    Score a{3, 4};
    Score b{3, 4};
    Score c{3, 5};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(ScoreTest, DefaultValues) {
    EXPECT_EQ((Score{}), (Score{0, 0}));
    EXPECT_EQ((Score{}), ZERO_SCORE);
}

TEST(ScoreTest, Operations) {
    Score a{3, 4};
    Score b{1, 2};

    EXPECT_EQ(a + b, (Score{4, 6}));  // Addition
    EXPECT_EQ(a - b, (Score{2, 2}));  // Subtraction
    EXPECT_EQ(a * 2, (Score{6, 8}));  // Multiplication
    EXPECT_EQ(-a, (Score{-3, -4}));   // Negation

    EXPECT_TRUE(a == (Score{3, 4}));   // Equality
    EXPECT_FALSE(a == (Score{4, 5}));  // Inequality
    EXPECT_TRUE(a != b);               // Inequality check
    EXPECT_TRUE(a < (Score{4, 5}));    // Less Than
}

TEST(ScoreTest, AssignmentOperators) {
    Score a{1, 2};

    // Addition Assignment
    a += Score{3, 4};
    EXPECT_EQ(a, (Score{4, 6}));

    // Subtraction Assignment
    a -= Score{1, 1};
    EXPECT_EQ(a, (Score{3, 5}));

    // Multiplication Assignment
    a *= 2;
    EXPECT_EQ(a, (Score{6, 10}));
}