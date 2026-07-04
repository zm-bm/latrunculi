#include "uci.hpp"

#include "board.hpp"
#include "evaluator.hpp"
#include "position_state.hpp"
#include "root_line.hpp"
#include "search_instrumentation.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <format>
#include <sstream>
#include <utility>

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

std::string lower_ascii(std::string value) {
    std::ranges::transform(
        value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
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
            limits.searchmoves = true;
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
            command.fen    = Board::startfen;
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

std::string format_search_info_fields(const SearchInfo& info) {
    return std::format("depth {} score {} nodes {} time {} nps {} pv {}",
                       info.depth,
                       info.score_text(),
                       info.nodes,
                       info.time.count(),
                       info.nps_text(),
                       info.pv);
}

std::string format_root_pv(const RootLine& line, const Board& root_board) {
    if (!line.usable_root_move() || line.pv.empty() || line.pv.front() != line.root_move)
        return "";

    PositionStateStack pv_states;
    Board              pv_board{pv_states.root()};
    pv_board.load_board(&root_board);

    std::string pv;
    for (int i = 0; i < line.pv.size(); ++i) {
        const Move move = line.pv.move_at(i);
        if (!pv_board.is_legal_move(move))
            return "";

        pv += move.str() + " ";

        if (i + 1 < line.pv.size())
            pv_board.make(move, pv_states.child(i));
    }

    return pv;
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
    if (command == "board")
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

std::string format_uci_move(Move move) {
    return move.is_null() ? "0000" : move.str();
}

std::string format_option(std::string_view name, const SpinOption& opt) {
    return std::format("option name {} type spin default {} min {} max {}",
                       name,
                       opt.def_value,
                       opt.min_value,
                       opt.max_value);
}

std::string format_option(std::string_view name, const CheckOption& opt) {
    return std::format(
        "option name {} type check default {}", name, opt.def_value ? "true" : "false");
}

std::string format_option(std::string_view name, const ButtonOption&) {
    return std::format("option name {} type button", name);
}

std::string format_identification(const uci::Config& config) {
    return std::format("id name Latrunculi {}\n"
                       "id author Eric VanderHelm\n"
                       "{}\n"
                       "{}\n"
                       "{}\n"
                       "{}\n"
                       "uciok",
                       LATRUNCULI_VERSION,
                       format_option("Hash", config.hash),
                       format_option("Clear Hash", config.clear_hash),
                       format_option("Threads", config.threads),
                       format_option("Debug", config.debug));
}

std::string format_ready() {
    return "readyok";
}

std::string format_bestmove(Move move) {
    return std::format("bestmove {}", format_uci_move(move));
}

std::string format_info(const uci::SearchInfo& info) {
    return std::format("info {}", format_search_info_fields(info));
}

std::string format_info_string(std::string_view str) {
    return std::format("info string {}", str);
}

void SpinOption::set(std::string value_str) {
    size_t pos = 0;
    int    val = std::stoi(value_str, &pos);
    while (pos < value_str.size() && std::isspace(static_cast<unsigned char>(value_str[pos])))
        ++pos;
    if (pos != value_str.size())
        throw std::invalid_argument("Invalid integer value");
    if (val < min_value || val > max_value)
        throw std::out_of_range("Value out of range");
    value = val;
}

void CheckOption::set(std::string val_str) {
    val_str = lower_ascii(std::move(val_str));
    if (val_str == "true" || val_str == "on") {
        value = true;
    } else if (val_str == "false" || val_str == "off") {
        value = false;
    } else {
        throw std::invalid_argument("Invalid boolean value");
    }
}

ConfigOption Config::set_option(const std::string& name, const std::string& value) {
    const std::string option_name = lower_ascii(name);

    if (option_name == "hash") {
        hash.set(value);
        return ConfigOption::Hash;
    } else if (option_name == "threads") {
        threads.set(value);
        return ConfigOption::Threads;
    } else if (option_name == "debug") {
        debug.set(value);
        return ConfigOption::Debug;
    } else if (option_name == "clear hash") {
        if (!value.empty())
            throw std::invalid_argument("Clear Hash does not take a value");
        return ConfigOption::ClearHash;
    } else {
        throw std::invalid_argument("Unknown UCI option: " + name);
    }
}

std::string SearchInfo::score_text() const {
    if (std::abs(score) > MATE_BOUND) {
        int mate_distance = MATE_VALUE - std::abs(score);
        int mate_in_n     = (mate_distance + 1) / 2;
        return "mate " + std::to_string(mate_in_n * (score > 0 ? 1 : -1));
    }
    return "cp " + std::to_string(score);
}

std::string SearchInfo::nps_text() const {
    auto count = time.count();
    auto nps   = count > 0 ? (nodes * 1000 / count) : 0;
    return std::to_string(nps);
}

SearchInfo
make_search_info(const RootLine& line, const Board& root_board, uint64_t nodes, Milliseconds time) {
    return SearchInfo{line.value, line.depth, nodes, time, format_root_pv(line, root_board)};
}

void Protocol::help() const {
    constexpr auto format_str = R"(
Available commands:
  uci           - Show engine identity and supported options
  isready       - Check if the engine is ready
  setoption     - Set engine options
  ucinewgame    - Start a new game
  position      - Set up the board position
  go            - Start searching for the best move
  stop          - Stop the search
  ponderhit     - Handle ponder hit
  quit          - Exit the engine
  perft <depth> - Run perft for the given depth
  move <move>   - Make a move on the board
  moves         - Show all legal moves
  d             - Display the current board position
  eval          - Evaluate the current position
For UCI protocol details, see: https://www.wbec-ridderkerk.nl/html/UCIProtocol.html)";
    err << format_str << '\n';
}

void Protocol::identify(const uci::Config& config) const {
    out << format_identification(config) << '\n';
    out.flush();
}

void Protocol::ready() const {
    out << format_ready() << '\n';
    out.flush();
}

void Protocol::bestmove(Move move) const {
    out << format_bestmove(move) << '\n';
    out.flush();
}

void Protocol::info(const std::string& str) const {
    out << format_info_string(str) << '\n';
    out.flush();
}

void Protocol::info(const uci::SearchInfo& info) const {
    out << format_info(info) << '\n';
    out.flush();
}

template <typename T>
void Protocol::debug(T&& obj) const {
    err << std::format("{}", std::forward<T>(obj)) << '\n';
}

template void Protocol::debug(std::string& str) const;
template void Protocol::debug(std::string&& str) const;
template void Protocol::debug(Board& board) const;
template void Protocol::debug(EvaluatorDebug& evaluator) const;
template void Protocol::debug(SearchInstrumentation<>& evaluator) const;

} // namespace uci
