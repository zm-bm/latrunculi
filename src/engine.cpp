#include "engine.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include "board.hpp"
#include "evaluator.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "search_options.hpp"
#include "tt.hpp"

namespace {

SearchOptions make_search_options(const uci::GoLimits& limits, Board& board) {
    SearchOptions options;
    options.board = &board;

    if (limits.depth)
        options.set_depth(*limits.depth);
    if (limits.movetime)
        options.set_movetime(*limits.movetime);
    if (limits.nodes)
        options.set_nodes(*limits.nodes);
    if (limits.wtime)
        options.set_wtime(*limits.wtime);
    if (limits.btime)
        options.set_btime(*limits.btime);
    if (limits.winc)
        options.set_winc(*limits.winc);
    if (limits.binc)
        options.set_binc(*limits.binc);
    if (limits.movestogo)
        options.set_movestogo(*limits.movestogo);

    return options;
}

std::string position_root_fen(const uci::PositionCommand& command) {
    using Source = uci::PositionCommand::Source;

    switch (command.source) {
    case Source::Startpos: return Board::startfen;
    case Source::Fen:
        if (!command.fen.empty())
            return command.fen;
        break;
    case Source::Invalid: break;
    }

    throw std::runtime_error("invalid position command");
}

} // namespace

Engine::Engine(std::ostream& out, std::ostream& err, std::istream& in)
    : out(out),
      err(err),
      in(in),
      writer(out, err),
      position_states{PositionState()},
      position_ply(0),
      board(position_states.front(), Board::startfen),
      thread_pool(DEFAULT_THREADS, writer) {}

PositionState& Engine::next_position_state() {
    if (position_states.size() <= position_ply + 1)
        position_states.emplace_back();
    return position_states[position_ply + 1];
}

void Engine::reset_board(const std::string& fen) {
    board.load_fen(fen);

    const auto root_state = board.position_state();
    position_states.clear();
    position_states.push_back(root_state);
    position_ply = 0;
    board.bind_position_state(position_states.front());
}

void Engine::make_board_move(Move move) {
    board.make(move, next_position_state());
    ++position_ply;
}

void Engine::unmake_board_move() {
    if (position_ply == 0)
        throw std::runtime_error("no move to undo");

    board.unmake(position_states[position_ply - 1]);
    --position_ply;
}

void Engine::apply_option_side_effect(uci::ConfigOption option) {
    switch (option) {
    case uci::ConfigOption::Hash:      tt.resize(config.hash.value); break;
    case uci::ConfigOption::Threads:   thread_pool.resize(config.threads.value); break;
    case uci::ConfigOption::Debug:     break;
    case uci::ConfigOption::ClearHash: tt.clear(); break;
    }
}

void Engine::loop() {
    std::string line;
    while (std::getline(in, line)) {
        if (!execute(line))
            break;
    }
}

bool Engine::execute(const std::string& line) noexcept {
    try {
        return dispatch(uci::parse_command(line));
    } catch (const std::exception& e) {
        writer.info("error: " + std::string(e.what()));
        return true;
    } catch (...) {
        writer.info("unknown error occurred");
        return true;
    }
}

bool Engine::dispatch(const uci::Command& command) {
    return std::visit(
        [this](const auto& parsed) -> bool {
            using Command = std::decay_t<decltype(parsed)>;
            if constexpr (std::is_same_v<Command, uci::EmptyCommand>)
                return empty(parsed);
            else if constexpr (std::is_same_v<Command, uci::UciCommand>)
                return uci(parsed);
            else if constexpr (std::is_same_v<Command, uci::DebugCommand>)
                return set_debug(parsed);
            else if constexpr (std::is_same_v<Command, uci::IsReadyCommand>)
                return is_ready(parsed);
            else if constexpr (std::is_same_v<Command, uci::SetOptionCommand>)
                return set_option(parsed);
            else if constexpr (std::is_same_v<Command, uci::NewGameCommand>)
                return new_game(parsed);
            else if constexpr (std::is_same_v<Command, uci::PositionCommand>)
                return position(parsed);
            else if constexpr (std::is_same_v<Command, uci::GoCommand>)
                return go(parsed);
            else if constexpr (std::is_same_v<Command, uci::StopCommand>)
                return stop(parsed);
            else if constexpr (std::is_same_v<Command, uci::PonderHitCommand>)
                return ponder_hit(parsed);
            else if constexpr (std::is_same_v<Command, uci::RegisterCommand>)
                return register_command(parsed);
            else if constexpr (std::is_same_v<Command, uci::QuitCommand>)
                return quit(parsed);
            else if constexpr (std::is_same_v<Command, uci::ExitCommand>)
                return exit(parsed);
            else if constexpr (std::is_same_v<Command, uci::ConsoleCommand>)
                return console(parsed);
            else if constexpr (std::is_same_v<Command, uci::UnknownCommand>)
                return unknown(parsed);
        },
        command);
}

bool Engine::uci(const uci::UciCommand&) {
    writer.identify(config);
    return true;
}

bool Engine::set_debug(const uci::DebugCommand& command) {
    apply_option_side_effect(config.set_option("Debug", command.value));
    return true;
}

bool Engine::is_ready(const uci::IsReadyCommand&) {
    writer.ready();
    return true;
}

bool Engine::set_option(const uci::SetOptionCommand& command) {
    if (thread_pool.is_searching())
        throw std::runtime_error("cannot set option while search is in progress");

    if (command.name.empty())
        throw std::runtime_error("missing option name");
    apply_option_side_effect(config.set_option(command.name, command.value));
    return true;
}

bool Engine::new_game(const uci::NewGameCommand&) {
    if (thread_pool.is_searching())
        throw std::runtime_error("cannot start new game while search is in progress");

    // UCI new game policy: drop all cached TT data so a fresh game starts from an empty table
    // with generation 0 instead of carrying entries across unrelated games.
    tt.clear();
    return true;
}

bool Engine::position(const uci::PositionCommand& command) {
    reset_board(position_root_fen(command));

    for (const auto& token : command.moves) {
        auto move = get_move(token);
        if (move.is_null())
            throw std::runtime_error("invalid move in position command: " + token);
        make_board_move(move);
    }
    return true;
}

bool Engine::go(const uci::GoCommand& command) {
    SearchOptions options = make_search_options(command.limits, board);
    if (!thread_pool.start_search(options))
        writer.info("search already in progress");
    return true;
}

bool Engine::stop(const uci::StopCommand&) {
    thread_pool.request_stop();
    return true;
}

bool Engine::quit(const uci::QuitCommand&) {
    thread_pool.shutdown();
    return false;
}

bool Engine::ponder_hit(const uci::PonderHitCommand&) {
    return true;
}

bool Engine::register_command(const uci::RegisterCommand&) {
    return true;
}

bool Engine::exit(const uci::ExitCommand&) {
    return quit(uci::QuitCommand{});
}

bool Engine::unknown(const uci::UnknownCommand& command) {
    return true;
}

bool Engine::empty(const uci::EmptyCommand&) {
    return true;
}

bool Engine::console(const uci::ConsoleCommand& command) {
    switch (command.name) {
    case uci::ConsoleCommand::Name::Help:  return help();
    case uci::ConsoleCommand::Name::Board: return display_board();
    case uci::ConsoleCommand::Name::Eval:  return evaluate();
    case uci::ConsoleCommand::Name::Move:  return move(command.arguments);
    case uci::ConsoleCommand::Name::Moves: return moves();
    case uci::ConsoleCommand::Name::Perft: return perft(command.arguments);
    }

    return true;
}

bool Engine::help() {
    writer.help();
    return true;
}

bool Engine::display_board() {
    writer.debug(board);
    return true;
}

bool Engine::evaluate() {
    EvaluatorDebug e{board};
    e.evaluate();
    writer.debug(e);
    return true;
}

bool Engine::move(const std::string& arguments) {
    std::istringstream stream(arguments);
    std::string        token;
    stream >> token;

    if (token == "undo") {
        unmake_board_move();
    } else {
        auto move = get_move(token);
        if (move.is_null()) {
            writer.info("invalid move: " + token);
        } else {
            make_board_move(move);
        }
    }
    return true;
}

bool Engine::moves() {
    auto movelist = movegen::generate_pseudo_legal(board);
    for (auto& move : movelist) {
        if (!board.is_legal_generated_move(move))
            continue;
        writer.debug(move.str());
    }
    return true;
}

bool Engine::perft(const std::string& arguments) {
    std::istringstream stream(arguments);
    int                depth;

    if (stream >> depth) {
        depth = std::max(1, depth);
        board.perft(depth, err);
    }

    return true;
}

Move Engine::get_move(const std::string& token) {
    auto movelist = movegen::generate_pseudo_legal(board);
    for (auto& move : movelist) {
        if (move.str() == token && board.is_legal_generated_move(move)) {
            return move;
        }
    }
    return NULL_MOVE;
}
