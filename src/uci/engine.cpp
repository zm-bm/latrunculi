#include "uci/engine.hpp"

#include <sstream>
#include <stdexcept>
#include <variant>

#include "board/board.hpp"
#include "core/move.hpp"
#include "eval/evaluator.hpp"
#include "movegen/movegen.hpp"
#include "movegen/perft.hpp"
#include "search/search_limits.hpp"
#include "search/tt.hpp"

Engine::Engine(std::ostream& out, std::ostream& err, std::istream& source)
    : reader(source),
      writer(out, err),
      ply_states{PlyState()},
      position_ply(0),
      board(ply_states.front(), Board::startfen),
      thread_pool(options.threads.value, writer) {}

PlyState& Engine::next_ply_state() {
    if (ply_states.size() <= position_ply + 1)
        ply_states.emplace_back();
    return ply_states[position_ply + 1];
}

void Engine::reset_board(const std::string& fen) {
    PlyState root_state;
    Board    root_board(root_state, fen);

    board.copy_root_from(root_board, ply_states.front());
    position_ply = 0;
}

void Engine::make_board_move(Move move) {
    board.make(move, next_ply_state());
    ++position_ply;
}

void Engine::unmake_board_move() {
    if (position_ply == 0)
        throw std::runtime_error("no move to undo");

    board.unmake(ply_states[position_ply - 1]);
    --position_ply;
}

void Engine::apply_option_effect(uci::OptionId option) {
    switch (option) {
    case uci::OptionId::Hash:      tt.resize(options.hash.value); break;
    case uci::OptionId::Threads:   thread_pool.resize(options.threads.value); break;
    case uci::OptionId::Debug:     break;
    case uci::OptionId::ClearHash: tt.clear(); break;
    }
}

void Engine::loop() {
    while (auto command = reader.read_command()) {
        if (!execute(*command))
            break;
    }
}

bool Engine::execute(const std::string& line) noexcept {
    try {
        return execute(uci::parse_command(line));
    } catch (const std::exception& e) {
        writer.info_string("error: " + std::string(e.what()));
        return true;
    } catch (...) {
        writer.info_string("unknown error occurred");
        return true;
    }
}

bool Engine::execute(const uci::Command& command) noexcept {
    try {
        return dispatch(command);
    } catch (const std::exception& e) {
        writer.info_string("error: " + std::string(e.what()));
        return true;
    } catch (...) {
        writer.info_string("unknown error occurred");
        return true;
    }
}

bool Engine::dispatch(const uci::Command& command) {
    return std::visit([this](const auto& parsed) { return handle(parsed); }, command);
}

bool Engine::handle(const uci::UciCommand&) {
    writer.identify(options);
    return true;
}

bool Engine::handle(const uci::DebugCommand& command) {
    apply_option_effect(options.set("Debug", command.value, true));
    return true;
}

bool Engine::handle(const uci::IsReadyCommand&) {
    writer.ready();
    return true;
}

bool Engine::handle(const uci::SetOptionCommand& command) {
    if (thread_pool.is_searching())
        throw std::runtime_error("cannot set option while search is in progress");

    if (command.name.empty())
        throw std::runtime_error("missing option name");
    apply_option_effect(options.set(command.name, command.value, command.has_value));
    return true;
}

bool Engine::handle(const uci::NewGameCommand&) {
    if (thread_pool.is_searching())
        throw std::runtime_error("cannot start new game while search is in progress");

    // UCI new game policy: drop all cached TT data so a fresh game starts from an empty table
    // with generation 0 instead of carrying entries across unrelated games.
    tt.clear();
    return true;
}

bool Engine::handle(const uci::PositionCommand& command) {
    using Source = uci::PositionCommand::Source;

    switch (command.source) {
    case Source::Startpos: reset_board(Board::startfen); break;
    case Source::Fen:
        if (command.fen.empty())
            throw std::runtime_error("invalid position command");
        reset_board(command.fen);
        break;
    case Source::Invalid: throw std::runtime_error("invalid position command");
    }

    for (const auto& token : command.moves) {
        auto move = find_legal_move(token);
        if (move.is_null())
            throw std::runtime_error("invalid move in position command: " + token);
        make_board_move(move);
    }
    return true;
}

bool Engine::handle(const uci::GoCommand& command) {
    const auto&  go_limits = command.limits;
    SearchLimits limits;

    if (go_limits.depth)
        limits.set_depth(*go_limits.depth);
    if (go_limits.movetime)
        limits.set_movetime(*go_limits.movetime);
    if (go_limits.nodes)
        limits.set_nodes(*go_limits.nodes);
    if (go_limits.wtime)
        limits.set_wtime(*go_limits.wtime);
    if (go_limits.btime)
        limits.set_btime(*go_limits.btime);
    if (go_limits.winc)
        limits.set_winc(*go_limits.winc);
    if (go_limits.binc)
        limits.set_binc(*go_limits.binc);
    if (go_limits.movestogo)
        limits.set_movestogo(*go_limits.movestogo);

    if (!thread_pool.start_search(board, limits))
        writer.info_string("search already in progress");
    return true;
}

bool Engine::handle(const uci::StopCommand&) {
    thread_pool.request_stop();
    return true;
}

bool Engine::handle(const uci::QuitCommand&) {
    thread_pool.shutdown();
    return false;
}

bool Engine::handle(const uci::PonderHitCommand&) {
    return true;
}

bool Engine::handle(const uci::RegisterCommand&) {
    return true;
}

bool Engine::handle(const uci::ExitCommand&) {
    return handle(uci::QuitCommand{});
}

bool Engine::handle(const uci::UnknownCommand&) {
    return true;
}

bool Engine::handle(const uci::EmptyCommand&) {
    return true;
}

bool Engine::handle(const uci::ConsoleCommand& command) {
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
        auto move = find_legal_move(token);
        if (move.is_null()) {
            writer.info_string("invalid move: " + token);
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
        writer.debug(uci::format_uci_move(move));
    }
    return true;
}

bool Engine::perft(const std::string& arguments) {
    std::istringstream stream(arguments);
    int                depth;

    if (stream >> depth) {
        writer.debug_text(format_perft_result(perft_root(board, depth)));
    }

    return true;
}

Move Engine::find_legal_move(const std::string& token) {
    auto movelist = movegen::generate_pseudo_legal(board);
    for (auto& move : movelist) {
        if (uci::format_uci_move(move) == token && board.is_legal_generated_move(move)) {
            return move;
        }
    }
    return NULL_MOVE;
}
