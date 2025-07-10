#include "engine.hpp"

#include <chrono>
#include <functional>
#include <sstream>
#include <variant>

#include "board.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "search_options.hpp"
#include "thread.hpp"
#include "tt.hpp"

static CommandFunc makeCommand(Engine* engine, bool (Engine::*func)(std::istringstream&)) {
    return [engine, func](std::istringstream& iss) { return (engine->*func)(iss); };
}

Engine::Engine(std::ostream& out, std::ostream& err, std::istream& in)
    : out(out),
      err(err),
      in(in),
      uciHandler(out, err),
      config(),
      board(STARTFEN),
      threadpool(DEFAULT_THREADS, uciHandler),
      commandMap({
          // UCI commands
          {"uci", makeCommand(this, &Engine::uci)},
          {"debug", makeCommand(this, &Engine::setdebug)},
          {"isready", makeCommand(this, &Engine::isready)},
          {"setoption", makeCommand(this, &Engine::setoption)},
          {"ucinewgame", makeCommand(this, &Engine::newgame)},
          {"position", makeCommand(this, &Engine::position)},
          {"go", makeCommand(this, &Engine::go)},
          {"stop", makeCommand(this, &Engine::stop)},
          {"ponderhit", makeCommand(this, &Engine::ponderhit)},
          {"quit", makeCommand(this, &Engine::quit)},
          // Non-UCI commands
          {"help", makeCommand(this, &Engine::help)},
          {"board", makeCommand(this, &Engine::displayBoard)},
          {"eval", makeCommand(this, &Engine::evaluate)},
          {"move", makeCommand(this, &Engine::move)},
          {"moves", makeCommand(this, &Engine::moves)},
          {"perft", makeCommand(this, &Engine::perft)},
          {"exit", makeCommand(this, &Engine::quit)},
          {"", [](std::istringstream&) { return true; }},
      }) {
    auto updateThreads = [this](const int& value) { threadpool.resize(value); };
    config.registerOption<int>("Threads", DEFAULT_THREADS, 1, MAX_THREADS, updateThreads);

    auto updateHash = [](const int& value) { TT::table.resize(value); };
    config.registerOption<int>("Hash", DEFAULT_HASH_MB, 1, MAX_HASH_MB, updateHash);

    config.registerOption<bool>("Debug", DEFAULT_DEBUG);
}

void Engine::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!execute(line)) break;
    }
}

bool Engine::execute(const std::string& line) noexcept {
    try {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        auto it = commandMap.find(token);
        if (it != commandMap.end()) {
            return it->second(iss);
        } else {
            uciHandler.info("unknown command: " + token + ", type help for a list of commands");
            return true;
        }
    } catch (const std::exception& e) {
        uciHandler.info("Error: " + std::string(e.what()));
        return true;
    } catch (...) {
        uciHandler.info("Unknown error occurred");
        return true;
    }
}

bool Engine::uci(std::istringstream& iss) {
    uciHandler.identify(config);
    return true;
}

bool Engine::setdebug(std::istringstream& iss) {
    std::string token;
    iss >> token;
    config.setOption("Debug", token);
    return true;
}

bool Engine::isready(std::istringstream& iss) {
    uciHandler.ready();
    return true;
}

bool Engine::setoption(std::istringstream& iss) {
    auto [name, value] = parseSetOption(iss);
    if (name.empty()) throw std::runtime_error("missing option name");
    if (value.empty()) throw std::runtime_error("missing option value");
    config.setOption(name, value);
    return true;
}

bool Engine::newgame(std::istringstream& iss) {
    TT::table.age();
    threadpool.age();
    return true;
}

bool Engine::position(std::istringstream& iss) {
    auto [fen, moves] = parsePosition(iss);
    if (fen.empty()) throw std::runtime_error("invalid position command");
    board.loadFEN(fen);

    std::istringstream movesStream(moves);
    std::string token;
    while (movesStream >> token) {
        auto move = getMove(token);
        if (move == NullMove) break;
        board.make(move);
    }
    return true;
}

bool Engine::go(std::istringstream& iss) {
    SearchOptions options{iss};
    options.fen   = board.toFEN();
    options.debug = config.getOption<bool>("Debug");

    for (const auto& w : options.warnings) {
        uciHandler.info("invalid " + w.name + " value " + w.value);
    }

    threadpool.startAll(options);
    return true;
}

bool Engine::stop(std::istringstream& iss) {
    threadpool.stopAll();
    return true;
}

bool Engine::quit(std::istringstream& iss) {
    threadpool.exitAll();
    return false;
}

bool Engine::ponderhit(std::istringstream& iss) {
    // TODO: Implement pondering
    uciHandler.info("ponderhit received");
    return true;
}

bool Engine::help(std::istringstream& iss) {
    uciHandler.help();
    return true;
}

bool Engine::displayBoard(std::istringstream& iss) {
    uciHandler.logOutput(board);
    return true;
}

bool Engine::evaluate(std::istringstream& iss) {
    Eval<Verbose> eval{board};
    eval.evaluate();
    uciHandler.logOutput(eval);
    return true;
}

bool Engine::move(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "undo") {
        board.unmake();
    } else {
        auto move = getMove(token);
        if (move == NullMove) {
            uciHandler.info("invalid move: " + token);
        } else {
            board.make(move);
        }
    }
    return true;
}

bool Engine::moves(std::istringstream& iss) {
    MoveGenerator<MoveGenMode::All> moves{board};
    for (auto& move : moves) uciHandler.logOutput(move.str());
    return true;
}

bool Engine::perft(std::istringstream& iss) {
    int depth;

    if (!(iss >> depth) || depth <= 0 || depth >= MAX_DEPTH) {
        iss.clear();
        std::string invalid = iss.str().substr(iss.tellg());
        uciHandler.info("invalid perft value " + invalid);
        return true;
    }

    board.perft<NodeType::Root>(depth, err);
    return true;
}

std::pair<std::string, std::string> Engine::parsePosition(std::istringstream& iss) {
    std::string token, fen, moves;
    bool inFen = false, inMoves = false;

    while (iss >> token) {
        if (token == "startpos") {
            fen     = STARTFEN;
            inFen   = false;
            inMoves = false;
            continue;
        } else if (token == "fen") {
            inFen   = true;
            inMoves = false;
            continue;
        } else if (token == "moves") {
            inFen   = false;
            inMoves = true;
            continue;
        }

        if (inFen) {
            if (!fen.empty()) fen += ' ';
            fen += token;
        } else if (inMoves) {
            if (!moves.empty()) moves += ' ';
            moves += token;
        }
    }

    return {fen, moves};
}

std::pair<std::string, std::string> Engine::parseSetOption(std::istringstream& iss) {
    std::string token, name, value;
    bool inName = false, inValue = false;

    while (iss >> token) {
        if (token == "name") {
            inName  = true;
            inValue = false;
            continue;
        } else if (token == "value") {
            inName  = false;
            inValue = true;
            continue;
        }

        if (inName) {
            if (!name.empty()) name += ' ';
            name += token;
        } else if (inValue) {
            if (!value.empty()) value += ' ';
            value += token;
        }
    }

    return {name, value};
}

Move Engine::getMove(const std::string& token) {
    MoveGenerator<MoveGenMode::All> moves{board};
    for (auto& move : moves) {
        if (move.str() == token && board.isLegalMove(move)) {
            return move;
        }
    }
    return NullMove;
}
