#include "uci/engine.hpp"

#include <algorithm>
#include <format>
#include <sstream>
#include <stdexcept>
#include <variant>
#include <vector>

#include "board/board.hpp"
#include "board/notation.hpp"
#include "core/move.hpp"
#include "eval/evaluator.hpp"
#include "movegen/movegen.hpp"
#include "movegen/perft.hpp"
#include "search/search_limits.hpp"
#include "search/tt.hpp"

namespace uci {

Engine::Engine(std::ostream& out, std::ostream& err, std::istream& source)
    : reader(source),
      writer(out, err),
      thread_pool(options.threads.value, writer) {}

void Engine::make_board_move(Move move) {
    board.make(move);
}

void Engine::unmake_board_move() {
    if (!board.can_unmake())
        throw std::runtime_error("no move to undo");

    board.unmake();
}

void Engine::apply_option_effect(OptionId option, const Options& candidate) {
    switch (option) {
    case OptionId::Hash: tt.resize(candidate.hash.value); break;
    case OptionId::Threads:
        if (!thread_pool.resize(candidate.threads.value))
            throw std::runtime_error("failed to resize thread pool");
        break;
    case OptionId::Ponder:    break;
    case OptionId::ClearHash: tt.clear(); break;
    }
}

void Engine::require_idle(std::string_view action) const {
    // Active searches retain their original board and configuration until completion.
    // Lifecycle-safe protocol commands are handled without this guard.
    if (thread_pool.is_searching())
        throw std::runtime_error("cannot " + std::string(action) + " while search is in progress");
}

void Engine::loop() {
    while (auto command = reader.read_command()) {
        if (!execute(*command))
            break;
    }
}

bool Engine::execute(const std::string& line) noexcept {
    try {
        return execute(parse_command(line));
    } catch (const std::exception& e) {
        writer.info_string("error: " + std::string(e.what()));
        return true;
    } catch (...) {
        writer.info_string("unknown error occurred");
        return true;
    }
}

bool Engine::execute(const Command& command) noexcept {
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

bool Engine::dispatch(const Command& command) {
    return std::visit([this](const auto& parsed) { return handle(parsed); }, command);
}

bool Engine::handle(const UciCommand&) {
    writer.identify(options);
    return true;
}

bool Engine::handle(const DebugCommand& command) {
    if (command.value == "on") {
        debug_mode = true;
        writer.info_string("debug mode enabled");
    } else if (command.value == "off") {
        debug_mode = false;
        writer.info_string("debug mode disabled");
    } else {
        throw std::runtime_error("invalid debug value: " + command.value);
    }
    return true;
}

bool Engine::handle(const IsReadyCommand&) {
    writer.ready();
    return true;
}

bool Engine::handle(const SetOptionCommand& command) {
    require_idle("set option");

    if (command.name.empty())
        throw std::runtime_error("missing option name");

    auto           candidate = options;
    const OptionId option    = candidate.set(command.name, command.value, command.has_value);
    apply_option_effect(option, candidate);
    options = candidate;
    return true;
}

bool Engine::handle(const NewGameCommand&) {
    require_idle("start new game");

    // Do not carry search heuristics or TT entries across unrelated games.
    thread_pool.clear_search_heuristics();
    tt.clear();
    return true;
}

bool Engine::handle(const PositionCommand& command) {
    using Source = PositionCommand::Source;

    require_idle("set position");

    Board candidate;

    switch (command.source) {
    case Source::Startpos: candidate.load_fen(Board::start_fen); break;
    case Source::Fen:
        if (command.fen.empty())
            throw std::runtime_error("invalid position command");
        candidate.load_fen(command.fen);
        break;
    case Source::Invalid: throw std::runtime_error("invalid position command");
    }

    for (const auto& token : command.moves) {
        auto move = find_legal_move(candidate, token);
        if (move.is_null())
            throw std::runtime_error("invalid move in position command: " + token);
        candidate.make(move);
    }

    board = candidate;
    if (debug_mode)
        writer.info_string("debug position " + board.to_fen());
    return true;
}

bool Engine::handle(const GoCommand& command) {
    if (thread_pool.is_searching()) {
        writer.info_string("search already in progress");
        return true;
    }

    const auto&  go_limits = command.limits;
    SearchLimits limits;

    limits.infinite = go_limits.infinite;
    limits.ponder   = go_limits.ponder;

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
    if (go_limits.mate)
        limits.set_mate(*go_limits.mate);
    if (go_limits.searchmoves)
        limits.set_root_moves(resolve_searchmoves(*go_limits.searchmoves));

    if (!thread_pool.start_search(board, limits))
        writer.info_string("search already in progress");
    return true;
}

bool Engine::handle(const StopCommand&) {
    thread_pool.request_stop();
    return true;
}

bool Engine::handle(const QuitCommand&) {
    thread_pool.shutdown();
    return false;
}

bool Engine::handle(const PonderHitCommand&) {
    thread_pool.leave_pondering();
    return true;
}

bool Engine::handle(const RegisterCommand&) {
    return true;
}

bool Engine::handle(const ExitCommand&) {
    return handle(QuitCommand{});
}

bool Engine::handle(const UnknownCommand&) {
    return true;
}

bool Engine::handle(const EmptyCommand&) {
    return true;
}

bool Engine::handle(const ConsoleCommand& command) {
    require_idle("run console command");

    switch (command.name) {
    case ConsoleCommand::Name::Help:  return help();
    case ConsoleCommand::Name::Board: return display_board();
    case ConsoleCommand::Name::Eval:  return evaluate();
    case ConsoleCommand::Name::Move:  return move(command.arguments);
    case ConsoleCommand::Name::Moves: return moves();
    case ConsoleCommand::Name::Perft: return perft(command.arguments);
    }

    return true;
}

bool Engine::help() {
    writer.help();
    return true;
}

bool Engine::display_board() {
    writer.diagnostic_line(std::format("{}", board));
    return true;
}

bool Engine::evaluate() {
    EvaluatorDebug e{board};
    e.evaluate();
    writer.diagnostic_line(std::format("{}", e));
    return true;
}

bool Engine::move(const std::string& arguments) {
    std::istringstream stream(arguments);
    std::string        token;
    stream >> token;

    if (token == "undo") {
        unmake_board_move();
    } else {
        auto move = find_legal_move(board, token);
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
        if (!board.is_legal_pseudo_move(move))
            continue;
        writer.diagnostic_line(format_uci_move(move));
    }
    return true;
}

bool Engine::perft(const std::string& arguments) {
    std::istringstream stream(arguments);
    int                depth;

    if (stream >> depth) {
        writer.diagnostic_text(format_perft_result(perft_root(board, depth)));
    }

    return true;
}

Move Engine::find_legal_move(const Board& position, const std::string& token) const {
    auto movelist = movegen::generate_pseudo_legal(position);
    for (auto& move : movelist) {
        if (format_uci_move(move) == token && position.is_legal_pseudo_move(move)) {
            return move;
        }
    }
    return NULL_MOVE;
}

std::vector<Move> Engine::resolve_searchmoves(const std::vector<std::string>& tokens) const {
    if (tokens.empty())
        throw std::runtime_error("missing searchmoves");

    std::vector<Move> root_moves;
    for (const auto& token : tokens) {
        const Move move = find_legal_move(board, token);
        if (move.is_null())
            throw std::runtime_error("invalid searchmove: " + token);
        if (std::find(root_moves.begin(), root_moves.end(), move) == root_moves.end())
            root_moves.push_back(move);
    }
    return root_moves;
}

} // namespace uci
