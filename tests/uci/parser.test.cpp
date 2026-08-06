#include "uci/parser.hpp"

#include <limits>
#include <string>
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

TEST(UciParserTest, ParsesCoreUciCommands) {
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

TEST(UciParserTest, ParsesSetOptionNameAndValueWithSpaces) {
    const auto command =
        parse_as<uci::SetOptionCommand>("setoption name Clear Hash value reset now");

    EXPECT_EQ(command.name, "Clear Hash");
    EXPECT_TRUE(command.has_value);
    EXPECT_EQ(command.value, "reset now");
}

TEST(UciParserTest, ParsesSetOptionButtonWithoutValue) {
    const auto command = parse_as<uci::SetOptionCommand>("setoption name Clear Hash");

    EXPECT_EQ(command.name, "Clear Hash");
    EXPECT_FALSE(command.has_value);
    EXPECT_TRUE(command.value.empty());
}

TEST(UciParserTest, ParsesPositionStartposMoves) {
    const auto command = parse_as<uci::PositionCommand>("\tposition\tstartpos\tmoves\te2e4 e7e5 ");

    EXPECT_EQ(command.source, uci::PositionCommand::Source::Startpos);
    EXPECT_TRUE(command.fen.empty());
    ASSERT_EQ(command.moves.size(), 2U);
    EXPECT_EQ(command.moves[0], "e2e4");
    EXPECT_EQ(command.moves[1], "e7e5");
}

TEST(UciParserTest, ParsesPositionFenMoves) {
    const auto command =
        parse_as<uci::PositionCommand>("position fen 8/8/8/8/8/8/8/8 w - - 0 1 moves a1a2");

    EXPECT_EQ(command.source, uci::PositionCommand::Source::Fen);
    EXPECT_EQ(command.fen, "8/8/8/8/8/8/8/8 w - - 0 1");
    ASSERT_EQ(command.moves.size(), 1U);
    EXPECT_EQ(command.moves[0], "a1a2");
}

TEST(UciParserTest, ParsesGoSupportedLimits) {
    const auto command = parse_as<uci::GoCommand>(
        "go depth 3 movetime 20 nodes 1000 wtime 3000 btime 4000 winc 12 binc 13 movestogo 5");

    EXPECT_EQ(command.limits.depth, 3);
    EXPECT_EQ(command.limits.movetime, Milliseconds::rep{20});
    EXPECT_EQ(command.limits.nodes, NodeCount{1000});
    EXPECT_EQ(command.limits.wtime, Milliseconds::rep{3000});
    EXPECT_EQ(command.limits.btime, Milliseconds::rep{4000});
    EXPECT_EQ(command.limits.winc, Milliseconds::rep{12});
    EXPECT_EQ(command.limits.binc, Milliseconds::rep{13});
    EXPECT_EQ(command.limits.movestogo, 5);
}

TEST(UciParserTest, ParsesGoNumericBoundaries) {
    using Rep = Milliseconds::rep;

    constexpr NodeCount wide_value = NodeCount{1} << 40;
    constexpr NodeCount max_value  = std::numeric_limits<NodeCount>::max();

    const auto zero = parse_as<uci::GoCommand>("go nodes 0");
    const auto wide = parse_as<uci::GoCommand>("go nodes +" + std::to_string(wide_value));
    const auto max  = parse_as<uci::GoCommand>("go nodes " + std::to_string(max_value));

    EXPECT_EQ(zero.limits.nodes, NodeCount{0});
    EXPECT_EQ(wide.limits.nodes, wide_value);
    EXPECT_EQ(max.limits.nodes, max_value);

    const auto signed_limits =
        parse_as<uci::GoCommand>("go depth " + std::to_string(std::numeric_limits<int>::max())
                                 + " movetime " + std::to_string(std::numeric_limits<Rep>::max()));

    EXPECT_EQ(signed_limits.limits.depth, std::numeric_limits<int>::max());
    EXPECT_EQ(signed_limits.limits.movetime, std::numeric_limits<Rep>::max());
}

TEST(UciParserTest, RejectsInvalidGoNumericValuesWithoutConsumingLaterKeywords) {
    constexpr std::string_view invalid_values[] = {
        "-1",
        "-0",
        "18446744073709551616",
        "+-1",
        "1000nodes",
        "many",
    };

    for (const auto value : invalid_values) {
        SCOPED_TRACE(value);
        const auto command = parse_as<uci::GoCommand>("go nodes " + std::string(value));

        EXPECT_FALSE(command.limits.nodes.has_value());
        EXPECT_EQ(command.limits.unknown_tokens, std::vector<std::string>{std::string(value)});
    }

    const auto missing = parse_as<uci::GoCommand>("go nodes");
    EXPECT_FALSE(missing.limits.nodes.has_value());
    EXPECT_TRUE(missing.limits.unknown_tokens.empty());

    constexpr NodeCount above_int_max = static_cast<NodeCount>(std::numeric_limits<int>::max()) + 1;
    const auto          out_of_range  = parse_as<uci::GoCommand>(
        "go depth " + std::to_string(above_int_max) + " movetime 999999999999999999999999999999");

    EXPECT_FALSE(out_of_range.limits.depth.has_value());
    EXPECT_FALSE(out_of_range.limits.movetime.has_value());

    const auto recovery = parse_as<uci::GoCommand>("go depth nodes 42 movetime -50");
    EXPECT_FALSE(recovery.limits.depth.has_value());
    EXPECT_EQ(recovery.limits.nodes, NodeCount{42});
    EXPECT_EQ(recovery.limits.movetime, Milliseconds::rep{-50});
    EXPECT_TRUE(recovery.limits.unknown_tokens.empty());
}

TEST(UciParserTest, RecordsUnsupportedGoAndTerminalSearchmoves) {
    const auto command =
        parse_as<uci::GoCommand>("go infinite ponder mate 4 unknown searchmoves e2e4 d2d4");

    EXPECT_TRUE(command.limits.infinite);
    EXPECT_TRUE(command.limits.ponder);
    EXPECT_EQ(command.limits.mate, 4);
    ASSERT_EQ(command.limits.unknown_tokens.size(), 1U);
    EXPECT_EQ(command.limits.unknown_tokens[0], "unknown");
    ASSERT_TRUE(command.limits.searchmoves.has_value());
    EXPECT_EQ(*command.limits.searchmoves, (std::vector<std::string>{"e2e4", "d2d4"}));

    const auto absent = parse_as<uci::GoCommand>("go depth 1");
    EXPECT_FALSE(absent.limits.searchmoves.has_value());

    const auto empty = parse_as<uci::GoCommand>("go searchmoves");
    ASSERT_TRUE(empty.limits.searchmoves.has_value());
    EXPECT_TRUE(empty.limits.searchmoves->empty());

    const auto terminal = parse_as<uci::GoCommand>("go searchmoves e2e4 depth 3");
    ASSERT_TRUE(terminal.limits.searchmoves.has_value());
    EXPECT_EQ(*terminal.limits.searchmoves, (std::vector<std::string>{"e2e4", "depth", "3"}));
    EXPECT_FALSE(terminal.limits.depth.has_value());
}

TEST(UciParserTest, ParsesDebugExtensionCommands) {
    const auto command = parse_as<uci::ConsoleCommand>("perft 3");

    EXPECT_EQ(command.name, uci::ConsoleCommand::Name::Perft);
    EXPECT_EQ(command.arguments, "3");
}

TEST(UciParserTest, ParsesBoardDisplayAlias) {
    const auto command = parse_as<uci::ConsoleCommand>("d");

    EXPECT_EQ(command.name, uci::ConsoleCommand::Name::Board);
    EXPECT_TRUE(command.arguments.empty());
}

TEST(UciParserTest, UnknownPrefixesRecoverWithoutRedispatchingPayload) {
    EXPECT_TRUE(
        std::holds_alternative<uci::IsReadyCommand>(uci::parse_command("noise ignored isready")));

    const auto unknown = parse_as<uci::UnknownCommand>("notacommand arg");
    const auto debug   = parse_as<uci::DebugCommand>("debug joho isready");
    const auto option = parse_as<uci::SetOptionCommand>("noise setoption name Debug value isready");

    EXPECT_EQ(unknown.token, "notacommand");
    EXPECT_EQ(debug.value, "joho");
    EXPECT_EQ(option.name, "Debug");
    EXPECT_TRUE(option.has_value);
    EXPECT_EQ(option.value, "isready");
}
