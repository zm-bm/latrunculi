#include "uci/uci_input.hpp"

#include <sstream>
#include <string_view>
#include <variant>

#include <gtest/gtest.h>

namespace {

template <typename T>
T parse_as(std::string_view line) {
    auto command = uci::parse_command(line);
    EXPECT_TRUE(std::holds_alternative<T>(command));
    return std::get<T>(command);
}

} // namespace

TEST(UciInputTest, ReaderPreservesBlankLineAsEmptyCommand) {
    std::istringstream input{" \t \n"};
    uci::Reader        reader{input};

    auto command = reader.read_command();

    ASSERT_TRUE(command.has_value());
    EXPECT_TRUE(std::holds_alternative<uci::EmptyCommand>(*command));
}

TEST(UciInputTest, ReaderReturnsNulloptAtEof) {
    std::istringstream input;
    uci::Reader        reader{input};

    EXPECT_FALSE(reader.read_command().has_value());
}

TEST(UciInputTest, ReaderReadsMultipleLinesInOrder) {
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

TEST(UciInputTest, ParsesCoreUciCommands) {
    EXPECT_TRUE(std::holds_alternative<uci::UciCommand>(uci::parse_command("uci")));
    EXPECT_TRUE(std::holds_alternative<uci::DebugCommand>(uci::parse_command("debug on")));
    EXPECT_TRUE(std::holds_alternative<uci::IsReadyCommand>(uci::parse_command("isready")));
    EXPECT_TRUE(std::holds_alternative<uci::SetOptionCommand>(
        uci::parse_command("setoption name Hash value 32")));
    EXPECT_TRUE(std::holds_alternative<uci::NewGameCommand>(uci::parse_command("ucinewgame")));
    EXPECT_TRUE(
        std::holds_alternative<uci::PositionCommand>(uci::parse_command("position startpos")));
    EXPECT_TRUE(std::holds_alternative<uci::GoCommand>(uci::parse_command("go depth 3")));
    EXPECT_TRUE(std::holds_alternative<uci::StopCommand>(uci::parse_command("stop")));
    EXPECT_TRUE(std::holds_alternative<uci::PonderHitCommand>(uci::parse_command("ponderhit")));
    EXPECT_TRUE(std::holds_alternative<uci::RegisterCommand>(uci::parse_command("register later")));
    EXPECT_TRUE(std::holds_alternative<uci::QuitCommand>(uci::parse_command("quit")));
    EXPECT_TRUE(std::holds_alternative<uci::ExitCommand>(uci::parse_command("exit")));
    EXPECT_TRUE(std::holds_alternative<uci::EmptyCommand>(uci::parse_command(" \t ")));
}

TEST(UciInputTest, ParsesSetOptionNameAndValueWithSpaces) {
    const auto command =
        parse_as<uci::SetOptionCommand>("setoption name Clear Hash value reset now");

    EXPECT_EQ(command.name, "Clear Hash");
    EXPECT_TRUE(command.has_value);
    EXPECT_EQ(command.value, "reset now");
}

TEST(UciInputTest, ParsesSetOptionButtonWithoutValue) {
    const auto command = parse_as<uci::SetOptionCommand>("setoption name Clear Hash");

    EXPECT_EQ(command.name, "Clear Hash");
    EXPECT_FALSE(command.has_value);
    EXPECT_TRUE(command.value.empty());
}

TEST(UciInputTest, ParsesPositionStartposMoves) {
    const auto command = parse_as<uci::PositionCommand>("\tposition\tstartpos\tmoves\te2e4 e7e5 ");

    EXPECT_EQ(command.source, uci::PositionCommand::Source::Startpos);
    EXPECT_TRUE(command.fen.empty());
    ASSERT_EQ(command.moves.size(), 2U);
    EXPECT_EQ(command.moves[0], "e2e4");
    EXPECT_EQ(command.moves[1], "e7e5");
}

TEST(UciInputTest, ParsesPositionFenMoves) {
    const auto command =
        parse_as<uci::PositionCommand>("position fen 8/8/8/8/8/8/8/8 w - - 0 1 moves a1a2");

    EXPECT_EQ(command.source, uci::PositionCommand::Source::Fen);
    EXPECT_EQ(command.fen, "8/8/8/8/8/8/8/8 w - - 0 1");
    ASSERT_EQ(command.moves.size(), 1U);
    EXPECT_EQ(command.moves[0], "a1a2");
}

TEST(UciInputTest, ParsesGoSupportedLimits) {
    const auto command = parse_as<uci::GoCommand>(
        "go depth 3 movetime 20 nodes 1000 wtime 3000 btime 4000 winc 12 binc 13 movestogo 5");

    EXPECT_EQ(command.limits.depth, 3);
    EXPECT_EQ(command.limits.movetime, 20);
    EXPECT_EQ(command.limits.nodes, 1000);
    EXPECT_EQ(command.limits.wtime, 3000);
    EXPECT_EQ(command.limits.btime, 4000);
    EXPECT_EQ(command.limits.winc, 12);
    EXPECT_EQ(command.limits.binc, 13);
    EXPECT_EQ(command.limits.movestogo, 5);
}

TEST(UciInputTest, IgnoresInvalidGoNumericValues) {
    const auto command = parse_as<uci::GoCommand>("go depth abc movetime -50 movestogo twenty");

    EXPECT_FALSE(command.limits.depth.has_value());
    EXPECT_EQ(command.limits.movetime, -50);
    EXPECT_FALSE(command.limits.movestogo.has_value());
    ASSERT_EQ(command.limits.unknown_tokens.size(), 2U);
    EXPECT_EQ(command.limits.unknown_tokens[0], "abc");
    EXPECT_EQ(command.limits.unknown_tokens[1], "twenty");
}

TEST(UciInputTest, RecordsUnsupportedAndUnknownGoTokens) {
    const auto command =
        parse_as<uci::GoCommand>("go infinite ponder mate 4 unknown searchmoves e2e4 d2d4");

    EXPECT_TRUE(command.limits.infinite);
    EXPECT_TRUE(command.limits.ponder);
    EXPECT_EQ(command.limits.mate, 4);
    ASSERT_EQ(command.limits.unknown_tokens.size(), 1U);
    EXPECT_EQ(command.limits.unknown_tokens[0], "unknown");
    ASSERT_EQ(command.limits.searchmoves.size(), 2U);
    EXPECT_EQ(command.limits.searchmoves[0], "e2e4");
    EXPECT_EQ(command.limits.searchmoves[1], "d2d4");
}

TEST(UciInputTest, ParsesDebugExtensionCommands) {
    const auto command = parse_as<uci::ConsoleCommand>("perft 3");

    EXPECT_EQ(command.name, uci::ConsoleCommand::Name::Perft);
    EXPECT_EQ(command.arguments, "3");
}

TEST(UciInputTest, ParsesBoardDisplayAlias) {
    const auto command = parse_as<uci::ConsoleCommand>("d");

    EXPECT_EQ(command.name, uci::ConsoleCommand::Name::Board);
    EXPECT_TRUE(command.arguments.empty());
}

TEST(UciInputTest, ParsesUnknownCommand) {
    const auto command = parse_as<uci::UnknownCommand>("notacommand arg");

    EXPECT_EQ(command.token, "notacommand");
}
