#include "uci/reader.hpp"

#include <sstream>
#include <variant>

#include <gtest/gtest.h>

TEST(UciReaderTest, PreservesBlankLineAsEmptyCommand) {
    std::istringstream input{" \t \n"};
    uci::Reader        reader{input};

    auto command = reader.read_command();

    ASSERT_TRUE(command.has_value());
    EXPECT_TRUE(std::holds_alternative<uci::EmptyCommand>(*command));
}

TEST(UciReaderTest, ReturnsNulloptAtEof) {
    std::istringstream input;
    uci::Reader        reader{input};

    EXPECT_FALSE(reader.read_command().has_value());
}

TEST(UciReaderTest, ReadsMultipleLinesInOrder) {
    std::istringstream input{"isready\nquit\n"};
    uci::Reader        reader{input};

    auto first  = reader.read_command();
    auto second = reader.read_command();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(std::holds_alternative<uci::IsReadyCommand>(*first));
    EXPECT_TRUE(std::holds_alternative<uci::QuitCommand>(*second));
    EXPECT_FALSE(reader.read_command().has_value());
}
