#include "score.hpp"

#include <gtest/gtest.h>

TEST(ScoreTest, Operations) {
    Score a{3, 4};
    Score b{1, 2};

    EXPECT_EQ(a + b, (Score{4, 6}));
    EXPECT_EQ(a - b, (Score{2, 2}));
    EXPECT_EQ(a * 2, (Score{6, 8}));
    EXPECT_EQ(-a, (Score{-3, -4}));

    EXPECT_TRUE(a == (Score{3, 4}));
    EXPECT_FALSE(a == (Score{4, 5}));
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < (Score{4, 5}));
}

TEST(ScoreTest, AssignmentOperators) {
    Score a{1, 2};

    a += Score{3, 4};
    EXPECT_EQ(a, (Score{4, 6}));
    a -= Score{1, 1};
    EXPECT_EQ(a, (Score{3, 5}));
    a *= 2;
    EXPECT_EQ(a, (Score{6, 10}));
}
