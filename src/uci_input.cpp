#include "uci_input.hpp"

#include <sstream>

namespace uci {

namespace {

std::vector<std::string> tokenize(std::string_view line) {
    std::istringstream       stream{std::string(line)};
    std::vector<std::string> tokens;
    std::string              token;

    while (stream >> token)
        tokens.push_back(token);

    return tokens;
}

std::string join_tokens(const std::vector<std::string>& tokens, size_t first) {
    std::string joined;
    for (size_t i = first; i < tokens.size(); ++i) {
        if (!joined.empty())
            joined += ' ';
        joined += tokens[i];
    }
    return joined;
}

std::optional<int> parse_int_token(const std::string& token) {
    try {
        size_t pos   = 0;
        int    value = std::stoi(token, &pos);
        if (pos == token.size())
            return value;
    } catch (...) {
    }
    return std::nullopt;
}

SetOptionCommand parse_setoption_command(const std::vector<std::string>& tokens) {
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

GoLimits parse_go_limits(const std::vector<std::string>& tokens) {
    GoLimits limits;

    auto read_value = [&](size_t& index, std::optional<int>& target) {
        if (index + 1 >= tokens.size())
            return;

        auto value = parse_int_token(tokens[index + 1]);
        if (!value)
            return;

        target = *value;
        ++index;
    };

    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "depth")
            read_value(i, limits.depth);
        else if (token == "movetime")
            read_value(i, limits.movetime);
        else if (token == "nodes")
            read_value(i, limits.nodes);
        else if (token == "wtime")
            read_value(i, limits.wtime);
        else if (token == "btime")
            read_value(i, limits.btime);
        else if (token == "winc")
            read_value(i, limits.winc);
        else if (token == "binc")
            read_value(i, limits.binc);
        else if (token == "movestogo")
            read_value(i, limits.movestogo);
        else if (token == "searchmoves") {
            for (++i; i < tokens.size(); ++i)
                limits.searchmoves.push_back(tokens[i]);
            break;
        } else if (token == "ponder") {
            limits.ponder = true;
        } else if (token == "infinite") {
            limits.infinite = true;
        } else if (token == "mate") {
            read_value(i, limits.mate);
        } else {
            limits.unknown_tokens.push_back(token);
        }
    }

    return limits;
}

PositionCommand parse_position_command(const std::vector<std::string>& tokens) {
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

ConsoleCommand console_command(ConsoleCommand::Name name, const std::vector<std::string>& tokens) {
    return ConsoleCommand{.name = name, .arguments = join_tokens(tokens, 1)};
}

} // namespace

Command parse_command(std::string_view line) {
    const auto tokens = tokenize(line);
    if (tokens.empty())
        return EmptyCommand{};

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
        return GoCommand{.limits = parse_go_limits(tokens)};
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

    return UnknownCommand{.token = command};
}

std::optional<Command> Reader::read_command() {
    std::string line;
    if (!std::getline(in, line))
        return std::nullopt;
    return parse_command(line);
}

} // namespace uci
