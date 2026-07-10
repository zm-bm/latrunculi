#include "core/types.hpp"

#include <gtest/gtest.h>

TEST(TypesTest, ColorInvert) {
    EXPECT_EQ(WHITE, ~BLACK);
}
