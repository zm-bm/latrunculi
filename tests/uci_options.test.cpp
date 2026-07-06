#include "uci_options.hpp"

#include <stdexcept>

#include <gtest/gtest.h>

TEST(UciOptionsTest, SetUpdatesValuesAndReturnsOptionId) {
    uci::Options options;

    EXPECT_EQ(options.set("hash", "16", true), uci::OptionId::Hash);
    EXPECT_EQ(options.set("THREADS", "2", true), uci::OptionId::Threads);
    EXPECT_EQ(options.set("dEbUg", "ON", true), uci::OptionId::Debug);
    EXPECT_EQ(options.set("clear hash", "", false), uci::OptionId::ClearHash);

    EXPECT_EQ(options.hash.value, 16);
    EXPECT_EQ(options.threads.value, 2);
    EXPECT_TRUE(options.debug.value);
}

TEST(UciOptionsTest, RejectsMalformedOptionValues) {
    uci::Options options;

    EXPECT_THROW(options.set("Threads", "2 extra", true), std::invalid_argument);
    EXPECT_THROW(options.set("Threads", "", false), std::invalid_argument);
    EXPECT_THROW(options.set("Debug", "maybe", true), std::invalid_argument);
    EXPECT_THROW(options.set("Debug", "", false), std::invalid_argument);
    EXPECT_THROW(options.set("Clear Hash", "", true), std::invalid_argument);
    EXPECT_THROW(options.set("Clear Hash", "now", true), std::invalid_argument);
}
