#include "eval/tapered_score.hpp"

#include <gtest/gtest.h>

TEST(TaperedScoreTest, Operations) {
    eval::TaperedScore a{3, 4};
    eval::TaperedScore b{1, 2};

    EXPECT_EQ(a + b, (eval::TaperedScore{4, 6}));
    EXPECT_EQ(a - b, (eval::TaperedScore{2, 2}));
    EXPECT_EQ(a * 2, (eval::TaperedScore{6, 8}));
    EXPECT_EQ(-a, (eval::TaperedScore{-3, -4}));

    EXPECT_TRUE(a == (eval::TaperedScore{3, 4}));
    EXPECT_FALSE(a == (eval::TaperedScore{4, 5}));
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < (eval::TaperedScore{4, 5}));
}

TEST(TaperedScoreTest, AssignmentOperators) {
    eval::TaperedScore a{1, 2};

    a += eval::TaperedScore{3, 4};
    EXPECT_EQ(a, (eval::TaperedScore{4, 6}));
    a -= eval::TaperedScore{1, 1};
    EXPECT_EQ(a, (eval::TaperedScore{3, 5}));
    a *= 2;
    EXPECT_EQ(a, (eval::TaperedScore{6, 10}));
}
