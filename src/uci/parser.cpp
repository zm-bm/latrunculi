#include "uci/parser.hpp"

#include <charconv>
#include <concepts>
#include <span>
#include <sstream>
#include <system_error>
#include <utility>

namespace uci {

namespace {

using Tokens = std::span<const std::string>;

std::vector<std::string> tokenize(std::string_view line) {
    std::istringstream       stream{std::string(line)};
    std::vector<std::string> tokens;
    std::string              token;

    while (stream >> token)
        tokens.push_back(token);

    return tokens;
}

std::string join_tokens(Tokens tokens, size_t first) {
    std::string joined;
    for (size_t i = first; i < tokens.size(); ++i) {
        if (!joined.empty())
            joined += ' ';
        joined += tokens[i];
    }
    return joined;
}

template <std::integral T>
std::optional<T> parse_integer_token(std::string_view token) {
    if (token.starts_with('+')) {
        token.remove_prefix(1);
        if (token.empty() || token.starts_with('+') || token.starts_with('-'))
            return std::nullopt;
    }

    T value{};
    const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (ec != std::errc{} || ptr != token.data() + token.size())
        return std::nullopt;
    return value;
}

SetOptionCommand parse_setoption_command(Tokens tokens) {
    SetOptionCommand command;

    bool in_name  = false;
    bool in_value = false;

    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "name") {
            in_name  = true;
            in_value = false;
            continue;
        }
        if (token == "value") {
            command.has_value = true;
            in_name           = false;
            in_value          = true;
            continue;
        }

        if (in_name) {
            if (!command.name.empty())
                command.name += ' ';
            command.name += token;
        } else if (in_value) {
            if (!command.value.empty())
                command.value += ' ';
            command.value += token;
        }
    }

    return command;
}

GoParameters parse_go_parameters(Tokens tokens) {
    GoParameters parameters;

    auto read_value = [&]<typename T>(size_t& index, std::optional<T>& target) {
        if (index + 1 >= tokens.size())
            return;

        auto value = parse_integer_token<T>(tokens[index + 1]);
        if (!value)
            return;

        target = *value;
        ++index;
    };

    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "depth")
            read_value(i, parameters.depth);
        else if (token == "movetime")
            read_value(i, parameters.movetime);
        else if (token == "nodes")
            read_value(i, parameters.nodes);
        else if (token == "wtime")
            read_value(i, parameters.wtime);
        else if (token == "btime")
            read_value(i, parameters.btime);
        else if (token == "winc")
            read_value(i, parameters.winc);
        else if (token == "binc")
            read_value(i, parameters.binc);
        else if (token == "movestogo")
            read_value(i, parameters.movestogo);
        else if (token == "searchmoves") {
            // By convention, searchmoves is terminal and owns the rest of the line.
            parameters.searchmoves.emplace();
            for (++i; i < tokens.size(); ++i)
                parameters.searchmoves->push_back(tokens[i]);
            break;
        } else if (token == "ponder") {
            parameters.ponder = true;
        } else if (token == "infinite") {
            parameters.infinite = true;
        } else if (token == "mate") {
            read_value(i, parameters.mate);
        } else {
            parameters.unknown_tokens.push_back(token);
        }
    }

    return parameters;
}

PositionCommand parse_position_command(Tokens tokens) {
    PositionCommand command;

    bool in_fen   = false;
    bool in_moves = false;

    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "startpos") {
            command.source = PositionCommand::Source::Startpos;
            in_fen         = false;
            in_moves       = false;
            continue;
        }
        if (token == "fen") {
            command.source = PositionCommand::Source::Fen;
            command.fen.clear();
            in_fen   = true;
            in_moves = false;
            continue;
        }
        if (token == "moves") {
            in_fen   = false;
            in_moves = true;
            continue;
        }

        if (in_fen) {
            if (!command.fen.empty())
                command.fen += ' ';
            command.fen += token;
        } else if (in_moves) {
            command.moves.push_back(token);
        }
    }

    return command;
}

ConsoleCommand console_command(ConsoleCommand::Name name, Tokens tokens) {
    return ConsoleCommand{.name = name, .arguments = join_tokens(tokens, 1)};
}

std::optional<Command> try_parse_command(Tokens tokens) {
    const std::string& command = tokens.front();

    if (command == "uci")
        return UciCommand{};
    if (command == "debug")
        return DebugCommand{.value = tokens.size() > 1 ? tokens[1] : ""};
    if (command == "isready")
        return IsReadyCommand{};
    if (command == "setoption")
        return parse_setoption_command(tokens);
    if (command == "ucinewgame")
        return NewGameCommand{};
    if (command == "position")
        return parse_position_command(tokens);
    if (command == "go")
        return GoCommand{.parameters = parse_go_parameters(tokens)};
    if (command == "stop")
        return StopCommand{};
    if (command == "ponderhit")
        return PonderHitCommand{};
    if (command == "register")
        return RegisterCommand{};
    if (command == "quit")
        return QuitCommand{};
    if (command == "exit")
        return ExitCommand{};
    if (command == "help")
        return console_command(ConsoleCommand::Name::Help, tokens);
    if (command == "board" || command == "d")
        return console_command(ConsoleCommand::Name::Board, tokens);
    if (command == "eval")
        return console_command(ConsoleCommand::Name::Eval, tokens);
    if (command == "move")
        return console_command(ConsoleCommand::Name::Move, tokens);
    if (command == "moves")
        return console_command(ConsoleCommand::Name::Moves, tokens);
    if (command == "perft")
        return console_command(ConsoleCommand::Name::Perft, tokens);

    return std::nullopt;
}

} // namespace

Command parse_command(std::string_view line) {
    const auto tokens = tokenize(line);
    if (tokens.empty())
        return EmptyCommand{};

    // Ignore unknown leading tokens as required by UCI. Once recognized,
    // a command owns the remaining tokens; do not resume top-level scanning.
    const Tokens token_view{tokens};
    for (size_t first = 0; first < token_view.size(); ++first) {
        if (auto command = try_parse_command(token_view.subspan(first)))
            return std::move(*command);
    }

    return UnknownCommand{.token = tokens.front()};
}

} // namespace uci
