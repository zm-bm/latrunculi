#include "engine.hpp"

#include <functional>
#include <sstream>
#include <stdexcept>

#include "board.hpp"
#include "evaluator.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "search_options.hpp"
#include "tt.hpp"

static Engine::Command make_cmd(Engine* engine, bool (Engine::*func)(std::istringstream&)) {
    return [engine, func](std::istringstream& iss) { return (engine->*func)(iss); };
}

Engine::Engine(std::ostream& out, std::ostream& err, std::istream& in)
    : out(out),
      err(err),
      in(in),
      protocol(out, err),
      config({.hash_callback   = [this](int value) { tt.resize(value); },
              .thread_callback = [this](int value) { thread_pool.resize(value); }}),
      position_states{PositionState()},
      position_ply(0),
      board(position_states.front(), Board::startfen),
      thread_pool(DEFAULT_THREADS, protocol),
      command_map({
          // UCI commands
          {"uci", make_cmd(this, &Engine::uci)},
          {"debug", make_cmd(this, &Engine::set_debug)},
          {"isready", make_cmd(this, &Engine::is_ready)},
          {"setoption", make_cmd(this, &Engine::set_option)},
          {"ucinewgame", make_cmd(this, &Engine::new_game)},
          {"position", make_cmd(this, &Engine::position)},
          {"go", make_cmd(this, &Engine::go)},
          {"stop", make_cmd(this, &Engine::stop)},
          {"ponderhit", make_cmd(this, &Engine::ponder_hit)},
          {"quit", make_cmd(this, &Engine::quit)},
          // Non-UCI commands
          {"help", make_cmd(this, &Engine::help)},
          {"board", make_cmd(this, &Engine::display_board)},
          {"eval", make_cmd(this, &Engine::evaluate)},
          {"move", make_cmd(this, &Engine::move)},
          {"moves", make_cmd(this, &Engine::moves)},
          {"perft", make_cmd(this, &Engine::perft)},
          {"exit", make_cmd(this, &Engine::quit)},
          {"", [](std::istringstream&) { return true; }},
      }) {}

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

void Engine::loop() {
    std::string line;
    while (std::getline(in, line)) {
        if (!execute(line))
            break;
    }
}

bool Engine::execute(const std::string& line) noexcept {
    try {
        std::istringstream iss(line);
        std::string        token;
        iss >> token;

        auto it = command_map.find(token);
        if (it != command_map.end()) {
            return it->second(iss);
        } else {
            protocol.info("unknown command: " + token + ", type help for a list of commands");
            return true;
        }
    } catch (const std::exception& e) {
        protocol.info("error: " + std::string(e.what()));
        return true;
    } catch (...) {
        protocol.info("unknown error occurred");
        return true;
    }
}

bool Engine::uci(std::istringstream& iss) {
    protocol.identify(config);
    return true;
}

bool Engine::set_debug(std::istringstream& iss) {
    std::string token;
    iss >> token;
    config.set_option("Debug", token);
    return true;
}

bool Engine::is_ready(std::istringstream& iss) {
    protocol.ready();
    return true;
}

bool Engine::set_option(std::istringstream& iss) {
    if (thread_pool.is_searching())
        throw std::runtime_error("cannot set option while search is in progress");

    auto [name, value] = parse_option(iss);
    if (name.empty())
        throw std::runtime_error("missing option name");
    if (value.empty())
        throw std::runtime_error("missing option value");
    config.set_option(name, value);
    return true;
}

bool Engine::new_game(std::istringstream& iss) {
    if (thread_pool.is_searching())
        throw std::runtime_error("cannot start new game while search is in progress");

    // UCI new game policy: drop all cached TT data so a fresh game starts from an empty table
    // with generation 0 instead of carrying entries across unrelated games.
    tt.clear();
    return true;
}

bool Engine::position(std::istringstream& iss) {
    auto [fen, moves] = parse_position(iss);
    if (fen.empty())
        throw std::runtime_error("invalid position command");
    reset_board(fen);

    std::istringstream moves_stream(moves);
    std::string        token;
    while (moves_stream >> token) {
        auto move = get_move(token);
        if (move.is_null())
            throw std::runtime_error("invalid move in position command: " + token);
        make_board_move(move);
    }
    return true;
}

bool Engine::go(std::istringstream& iss) {
    SearchOptions options(iss, &board);
    if (!thread_pool.start_search(options))
        protocol.info("search already in progress");
    return true;
}

bool Engine::stop(std::istringstream& iss) {
    thread_pool.request_stop();
    return true;
}

bool Engine::quit(std::istringstream& iss) {
    thread_pool.shutdown();
    return false;
}

bool Engine::ponder_hit(std::istringstream& iss) {
    protocol.info("ponderhit received");
    return true;
}

bool Engine::help(std::istringstream& iss) {
    protocol.help();
    return true;
}

bool Engine::display_board(std::istringstream& iss) {
    protocol.debug(board);
    return true;
}

bool Engine::evaluate(std::istringstream& iss) {
    EvaluatorDebug e{board};
    e.evaluate();
    protocol.debug(e);
    return true;
}

bool Engine::move(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "undo") {
        unmake_board_move();
    } else {
        auto move = get_move(token);
        if (move.is_null()) {
            protocol.info("invalid move: " + token);
        } else {
            make_board_move(move);
        }
    }
    return true;
}

bool Engine::moves(std::istringstream& iss) {
    auto movelist = movegen::generate_pseudo_legal(board);
    for (auto& move : movelist) {
        if (!board.is_legal_generated_move(move))
            continue;
        protocol.debug(move.str());
    }
    return true;
}

bool Engine::perft(std::istringstream& iss) {
    int depth;

    if (iss >> depth) {
        depth = std::max(1, depth);
        board.perft(depth, err);
    }

    return true;
}

std::pair<std::string, std::string> Engine::parse_position(std::istringstream& iss) {
    std::string token;
    std::string fen;
    std::string moves;

    bool in_fen   = false;
    bool in_moves = false;

    while (iss >> token) {
        if (token == "startpos") {
            fen      = Board::startfen;
            in_fen   = false;
            in_moves = false;
            continue;
        } else if (token == "fen") {
            in_fen   = true;
            in_moves = false;
            continue;
        } else if (token == "moves") {
            in_fen   = false;
            in_moves = true;
            continue;
        }

        if (in_fen) {
            if (!fen.empty())
                fen += ' ';
            fen += token;
        } else if (in_moves) {
            if (!moves.empty())
                moves += ' ';
            moves += token;
        }
    }

    return {fen, moves};
}

std::pair<std::string, std::string> Engine::parse_option(std::istringstream& iss) {
    std::string token;
    std::string name;
    std::string value;

    bool in_name  = false;
    bool in_value = false;

    while (iss >> token) {
        if (token == "name") {
            in_name  = true;
            in_value = false;
            continue;
        } else if (token == "value") {
            in_name  = false;
            in_value = true;
            continue;
        }

        if (in_name) {
            if (!name.empty())
                name += ' ';
            name += token;
        } else if (in_value) {
            if (!value.empty())
                value += ' ';
            value += token;
        }
    }

    return {name, value};
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
