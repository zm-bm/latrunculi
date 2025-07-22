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

TEST(HistoryTableTest, Clear) {
    HistoryTable historyTable;
    historyTable.update(WHITE, E2, E4, 3);
    historyTable.clear();
    EXPECT_EQ(historyTable.get(WHITE, E2, E4), 0);
}

TEST(KillerMovesTest, AddAndRetrieve) {
    KillerMoves killerMoves;
    killerMoves.update(Move(E2, E4), 0);
    EXPECT_TRUE(killerMoves.isKiller(Move(E2, E4), 0));
    EXPECT_FALSE(killerMoves.isKiller(Move(E2, E3), 0));
    EXPECT_FALSE(killerMoves.isKiller(Move(E2, E4), 1));
}

TEST(KillerMovesTest, LimitSize) {
    KillerMoves killerMoves;
    killerMoves.update(Move(C2, C4), 0);
    killerMoves.update(Move(D2, D4), 0);
    killerMoves.update(Move(E2, E4), 0);
    EXPECT_FALSE(killerMoves.isKiller(Move(C2, C4), 0));
    EXPECT_TRUE(killerMoves.isKiller(Move(D2, D4), 0));
    EXPECT_TRUE(killerMoves.isKiller(Move(E2, E4), 0));
}

TEST(KillerMovesTest, Clear) {
    KillerMoves killerMoves;
    killerMoves.update(Move(E2, E4), 0);
    killerMoves.update(Move(E2, E4), 1);
    killerMoves.clear();
    EXPECT_FALSE(killerMoves.isKiller(Move(E2, E4), 0));
    EXPECT_FALSE(killerMoves.isKiller(Move(E2, E4), 1));
}