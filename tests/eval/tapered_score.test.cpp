#include "eval/tapered_score.hpp"

#include <gtest/gtest.h>

TEST(TaperedScoreTest, Operations) {
    TaperedScore a{3, 4};
    TaperedScore b{1, 2};

    EXPECT_EQ(a + b, (TaperedScore{4, 6}));
    EXPECT_EQ(a - b, (TaperedScore{2, 2}));
    EXPECT_EQ(a * 2, (TaperedScore{6, 8}));
    EXPECT_EQ(-a, (TaperedScore{-3, -4}));

    EXPECT_TRUE(a == (TaperedScore{3, 4}));
    EXPECT_FALSE(a == (TaperedScore{4, 5}));
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < (TaperedScore{4, 5}));
}

TEST(TaperedScoreTest, AssignmentOperators) {
    TaperedScore a{1, 2};

    a += TaperedScore{3, 4};
    EXPECT_EQ(a, (TaperedScore{4, 6}));
    a -= TaperedScore{1, 1};
    EXPECT_EQ(a, (TaperedScore{3, 5}));
    a *= 2;
    EXPECT_EQ(a, (TaperedScore{6, 10}));
}
