#include "engine.hpp"

#include <functional>
#include <sstream>

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
      board(Board::startfen),
      thread_pool(DEFAULT_THREADS, protocol),
      config({.hash_callback   = [this](const int& value) { tt.resize(value); },
              .thread_callback = [this](const int& value) { thread_pool.resize(value); }}),
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

void Engine::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
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
    auto [name, value] = parse_option(iss);
    if (name.empty())
        throw std::runtime_error("missing option name");
    if (value.empty())
        throw std::runtime_error("missing option value");
    config.set_option(name, value);
    return true;
}

bool Engine::new_game(std::istringstream& iss) {
    tt.age_table();
    return true;
}

bool Engine::position(std::istringstream& iss) {
    auto [fen, moves] = parse_position(iss);
    if (fen.empty())
        throw std::runtime_error("invalid position command");
    board.load_fen(fen);

    std::istringstream moves_stream(moves);
    std::string        token;
    while (moves_stream >> token) {
        auto move = get_move(token);
        if (move.is_null())
            break;
        board.make(move);
    }
    return true;
}

bool Engine::go(std::istringstream& iss) {
    SearchOptions options(iss, &board);
    thread_pool.start_all(options);
    return true;
}

bool Engine::stop(std::istringstream& iss) {
    thread_pool.stop_all();
    return true;
}

bool Engine::quit(std::istringstream& iss) {
    thread_pool.exit_all();
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
    protocol.diagnostic_output(board);
    return true;
}

bool Engine::evaluate(std::istringstream& iss) {
    EvaluatorDebug e{board};
    e.evaluate();
    protocol.diagnostic_output(e);
    return true;
}

bool Engine::move(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "undo") {
        board.unmake();
    } else {
        auto move = get_move(token);
        if (move.is_null()) {
            protocol.info("invalid move: " + token);
        } else {
            board.make(move);
        }
    }
    return true;
}

bool Engine::moves(std::istringstream& iss) {
    auto movelist = generate<ALL_MOVES>(board);
    for (auto& move : movelist)
        protocol.diagnostic_output(move.str());
    return true;
}

bool Engine::perft(std::istringstream& iss) {
    int depth;

    if (iss >> depth) {
        depth = std::max(1, depth);
        board.perft<ROOT>(depth, err);
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
    auto movelist = generate<ALL_MOVES>(board);
    for (auto& move : movelist) {
        if (move.str() == token && board.is_legal_move(move)) {
            return move;
        }
    }
    return NULL_MOVE;
}
