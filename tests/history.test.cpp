#include "history.hpp"

#include <gtest/gtest.h>

TEST(HistoryTableTest, UpdateAndRetrieve) {
    HistoryTable hist;
    hist.update(WHITE, E2, E4, 3);

    auto value = hist.get(WHITE, E2, E4);
    EXPECT_GT(value, 0);
    EXPECT_EQ(hist.get(BLACK, E2, E4), 0);
    EXPECT_EQ(hist.get(WHITE, E4, E2), 0);

    hist.update(WHITE, E2, E4, 2);
    EXPECT_GT(hist.get(WHITE, E2, E4), value);
}

TEST(HistoryTableTest, Age) {
    HistoryTable hist;
    hist.update(WHITE, E2, E4, 3);

    auto value = hist.get(WHITE, E2, E4);
    hist.age();
    EXPECT_GT(hist.get(WHITE, E2, E4), 0);
    EXPECT_LT(hist.get(WHITE, E2, E4), value);
}

TEST(HistoryTableTest, Clear) {
    HistoryTable hist;
    hist.update(WHITE, E2, E4, 3);
    hist.clear();
    EXPECT_EQ(hist.get(WHITE, E2, E4), 0);
}
