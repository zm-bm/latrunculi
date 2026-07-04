#include "uci.hpp"

#include <sstream>
#include <stdexcept>
#include <variant>

#include "root_line.hpp"
#include "test_util.hpp"

#include "gtest/gtest.h"

namespace {

PrincipalVariation pv_for_move(Move move) {
    PrincipalVariation pv;
    PrincipalVariation child;
    pv.update(move, child);
    return pv;
}

PrincipalVariation pv_for_line(Move first, Move second) {
    PrincipalVariation child = pv_for_move(second);
    PrincipalVariation pv;
    pv.update(first, child);
    return pv;
}

template <typename T>
T parse_as(std::string_view line) {
    auto command = uci::parse_command(line);
    EXPECT_TRUE(std::holds_alternative<T>(command));
    return std::get<T>(command);
}

} // namespace

class ProtocolTest : public ::testing::Test {
protected:
    std::ostringstream oss, err;
    uci::Protocol      protocol{oss, err};

    void SetUp() override {
        oss.str("");
        oss.clear();
        err.str("");
        err.clear();
    }
};

TEST_F(ProtocolTest, Help) {
    protocol.help();
    EXPECT_NE(err.str().find("Available commands"), std::string::npos);
}

TEST_F(ProtocolTest, Identify) {
    uci::Config config;
    protocol.identify(config);
    EXPECT_NE(oss.str().find("uciok"), std::string::npos);
    EXPECT_NE(oss.str().find("option name Debug type check default false"), std::string::npos);
}

TEST_F(ProtocolTest, ConfigSetOptionWorksWithoutCallbacks) {
    uci::Config config;

    EXPECT_NO_THROW(config.set_option("Hash", "16"));
    EXPECT_NO_THROW(config.set_option("Threads", "2"));
    EXPECT_NO_THROW(config.set_option("Debug", "on"));

    EXPECT_EQ(config.hash.value, 16);
    EXPECT_EQ(config.threads.value, 2);
    EXPECT_TRUE(config.debug.value);
}

TEST_F(ProtocolTest, ConfigRejectsMalformedOptionValues) {
    uci::Config config;

    EXPECT_THROW(config.set_option("Threads", "2 extra"), std::invalid_argument);
    EXPECT_THROW(config.set_option("Debug", "maybe"), std::invalid_argument);
}

TEST_F(ProtocolTest, Ready) {
    protocol.ready();
    EXPECT_EQ(oss.str(), "readyok\n");
}

TEST_F(ProtocolTest, Bestmove) {
    Move move{E2, E4};
    protocol.bestmove(move);
    EXPECT_EQ(oss.str(), "bestmove e2e4\n");
}

TEST_F(ProtocolTest, BestmoveFormatsNullMoveAsUciNullMove) {
    protocol.bestmove(NULL_MOVE);
    EXPECT_EQ(oss.str(), "bestmove 0000\n");
}

TEST_F(ProtocolTest, FormatBestmoveFormatsNullMoveAsUciNullMove) {
    EXPECT_EQ(uci::format_bestmove(NULL_MOVE), "bestmove 0000");
}

TEST_F(ProtocolTest, InfoSearchInfo) {
    uci::SearchInfo info{
        .score = 50,
        .depth = 10,
        .nodes = 1000,
        .time  = Milliseconds{100},
        .pv    = "e2e4 e7e5",
    };

    protocol.info(info);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score cp 50"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv e2e4 e7e5"), std::string::npos);
}

TEST_F(ProtocolTest, InfoMate) {
    uci::SearchInfo info{
        .score = MATE_VALUE - 4,
        .depth = 10,
        .nodes = 1000,
        .time  = Milliseconds{100},
        .pv    = "e2e4 e7e5",
    };

    protocol.info(info);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score mate 2"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv e2e4 e7e5"), std::string::npos);
}

TEST_F(ProtocolTest, InfoNegativeMate) {
    uci::SearchInfo info{
        .score = -(MATE_VALUE - 4),
        .depth = 10,
        .nodes = 1000,
        .time  = Milliseconds{100},
        .pv    = "e2e4 e7e5",
    };

    protocol.info(info);

    EXPECT_NE(oss.str().find("depth 10"), std::string::npos);
    EXPECT_NE(oss.str().find("score mate -2"), std::string::npos);
    EXPECT_NE(oss.str().find("nps 10"), std::string::npos);
    EXPECT_NE(oss.str().find("pv e2e4 e7e5"), std::string::npos);
}

TEST_F(ProtocolTest, MakeSearchInfoSerializesLegalRootPv) {
    TestBoard board{STARTFEN};
    RootLine  line{
         .root_move = Move{E2, E4},
         .value     = 20,
         .depth     = 2,
         .completed = true,
         .pv        = pv_for_line(Move{E2, E4}, Move{E7, E5}),
    };

    const auto info = uci::make_search_info(line, board, 1234, Milliseconds{56});

    EXPECT_EQ(info.score, line.value);
    EXPECT_EQ(info.depth, line.depth);
    EXPECT_EQ(info.nodes, 1234U);
    EXPECT_EQ(info.time, Milliseconds{56});
    EXPECT_EQ(info.pv, "e2e4 e7e5 ");
}

TEST_F(ProtocolTest, MakeSearchInfoClearsUnusableRootPv) {
    TestBoard board{STARTFEN};

    RootLine null_best{
        .root_move = NULL_MOVE,
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = true,
    };
    EXPECT_EQ(uci::make_search_info(null_best, board, 0, Milliseconds{0}).pv, "");

    RootLine incomplete{
        .root_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = false,
        .pv        = pv_for_move(Move{E2, E4}),
    };
    EXPECT_EQ(uci::make_search_info(incomplete, board, 0, Milliseconds{0}).pv, "");

    RootLine depth_zero{
        .root_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 0,
        .completed = true,
        .pv        = pv_for_move(Move{E2, E4}),
    };
    EXPECT_EQ(uci::make_search_info(depth_zero, board, 0, Milliseconds{0}).pv, "");
}

TEST_F(ProtocolTest, MakeSearchInfoRejectsStaleRootPv) {
    TestBoard board{STARTFEN};

    RootLine first_move_mismatch{
        .root_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 1,
        .completed = true,
        .pv        = pv_for_move(Move{D2, D4}),
    };
    EXPECT_EQ(uci::make_search_info(first_move_mismatch, board, 0, Milliseconds{0}).pv, "");

    RootLine illegal_child{
        .root_move = Move{E2, E4},
        .value     = DRAW_VALUE,
        .depth     = 2,
        .completed = true,
        .pv        = pv_for_line(Move{E2, E4}, Move{H1, H2}),
    };
    EXPECT_EQ(uci::make_search_info(illegal_child, board, 0, Milliseconds{0}).pv, "");
}

TEST_F(ProtocolTest, InfoString) {
    std::string info = "This is a test info string";
    protocol.info(info);
    EXPECT_NE(oss.str().find(info), std::string::npos);
}

TEST_F(ProtocolTest, DebugOutput) {
    std::string logMessage = "This is a log message";
    protocol.debug(logMessage);
    EXPECT_EQ(err.str(), logMessage + "\n");
}

TEST(UciCommandParserTest, ParsesCoreUciCommands) {
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
    EXPECT_TRUE(std::holds_alternative<uci::QuitCommand>(uci::parse_command("quit")));
    EXPECT_TRUE(std::holds_alternative<uci::ExitCommand>(uci::parse_command("exit")));
    EXPECT_TRUE(std::holds_alternative<uci::EmptyCommand>(uci::parse_command(" \t ")));
}

TEST(UciCommandParserTest, ParsesSetOptionNameAndValueWithSpaces) {
    const auto command =
        parse_as<uci::SetOptionCommand>("setoption name Clear Hash value reset now");

    EXPECT_EQ(command.name, "Clear Hash");
    EXPECT_TRUE(command.has_value);
    EXPECT_EQ(command.value, "reset now");
}

TEST(UciCommandParserTest, ParsesSetOptionButtonWithoutValue) {
    const auto command = parse_as<uci::SetOptionCommand>("setoption name Clear Hash");

    EXPECT_EQ(command.name, "Clear Hash");
    EXPECT_FALSE(command.has_value);
    EXPECT_TRUE(command.value.empty());
}

TEST(UciCommandParserTest, ParsesPositionStartposMoves) {
    const auto command = parse_as<uci::PositionCommand>("\tposition\tstartpos\tmoves\te2e4 e7e5 ");

    EXPECT_EQ(command.source, uci::PositionCommand::Source::Startpos);
    EXPECT_EQ(command.fen, Board::startfen);
    ASSERT_EQ(command.moves.size(), 2U);
    EXPECT_EQ(command.moves[0], "e2e4");
    EXPECT_EQ(command.moves[1], "e7e5");
}

TEST(UciCommandParserTest, ParsesPositionFenMoves) {
    const auto command =
        parse_as<uci::PositionCommand>("position fen 8/8/8/8/8/8/8/8 w - - 0 1 moves a1a2");

    EXPECT_EQ(command.source, uci::PositionCommand::Source::Fen);
    EXPECT_EQ(command.fen, "8/8/8/8/8/8/8/8 w - - 0 1");
    ASSERT_EQ(command.moves.size(), 1U);
    EXPECT_EQ(command.moves[0], "a1a2");
}

TEST(UciCommandParserTest, ParsesGoSupportedLimits) {
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

TEST(UciCommandParserTest, IgnoresInvalidGoNumericValues) {
    const auto command = parse_as<uci::GoCommand>("go depth abc movetime -50 movestogo twenty");

    EXPECT_FALSE(command.limits.depth.has_value());
    EXPECT_EQ(command.limits.movetime, -50);
    EXPECT_FALSE(command.limits.movestogo.has_value());
    ASSERT_EQ(command.limits.unknown_tokens.size(), 2U);
    EXPECT_EQ(command.limits.unknown_tokens[0], "abc");
    EXPECT_EQ(command.limits.unknown_tokens[1], "twenty");
}

TEST(UciCommandParserTest, RecordsUnsupportedAndUnknownGoTokens) {
    const auto command =
        parse_as<uci::GoCommand>("go infinite ponder mate 4 unknown searchmoves e2e4 d2d4");

    EXPECT_TRUE(command.limits.infinite);
    EXPECT_TRUE(command.limits.ponder);
    EXPECT_EQ(command.limits.mate, 4);
    ASSERT_EQ(command.limits.unknown_tokens.size(), 1U);
    EXPECT_EQ(command.limits.unknown_tokens[0], "unknown");
    EXPECT_TRUE(command.limits.searchmoves);
}

TEST(UciCommandParserTest, SeparatesConsoleCommandsFromUciCommands) {
    const auto command = parse_as<uci::ConsoleCommand>("perft 3");

    EXPECT_EQ(command.name, uci::ConsoleCommand::Name::Perft);
    EXPECT_EQ(command.arguments, "3");
}

TEST(UciCommandParserTest, ParsesUnknownCommand) {
    const auto command = parse_as<uci::UnknownCommand>("notacommand arg");

    EXPECT_EQ(command.token, "notacommand");
}
