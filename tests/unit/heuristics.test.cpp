#include "heuristics.hpp"

#include <gtest/gtest.h>

TEST(HistoryTableTest, UpdateAndRetrieve) {
    HistoryTable historyTable;
    historyTable.update(WHITE, E2, E4, 3);
    auto historyValue = historyTable.get(WHITE, E2, E4);
    EXPECT_GT(historyValue, 0);
    EXPECT_EQ(historyTable.get(BLACK, E2, E4), 0);
    EXPECT_EQ(historyTable.get(WHITE, E4, E2), 0);

    historyTable.update(WHITE, E2, E4, 2);
    EXPECT_GT(historyTable.get(WHITE, E2, E4), historyValue);
}

TEST(HistoryTableTest, Age) {
    HistoryTable historyTable;
    historyTable.update(WHITE, E2, E4, 3);
    auto historyValue = historyTable.get(WHITE, E2, E4);
    historyTable.age();
    EXPECT_GT(historyTable.get(WHITE, E2, E4), 0);
    EXPECT_LT(historyTable.get(WHITE, E2, E4), historyValue);
}

TEST(HistoryTableTest, Reset) {
    HistoryTable historyTable;
    historyTable.update(WHITE, E2, E4, 3);
    historyTable.reset();
    EXPECT_EQ(historyTable.get(WHITE, E2, E4), 0);
}
