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

Engine::Engine(std::ostream& out, std::istream& in)
    : in(in), out(out), uciHandler(out), board(STARTFEN), threadpool(DEFAULT_THREADS, uciHandler) {
    commandMap = {
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
    };
}

void Engine::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            if (!execute(line)) break;
        } catch (const std::exception& e) {
            std::cerr << line << " failed: " << e.what() << std::endl;
        }
    }
}

bool Engine::execute(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;
    if (token.empty()) return true;

    auto it = commandMap.find(token);
    if (it != commandMap.end()) {
        return it->second(iss);
    } else {
        uciHandler.info("unknown command: " + token + ", type help for a list of commands");
        return true;
    }
}

bool Engine::uci(std::istringstream& iss) {
    uciHandler.identify();
    return true;
}

bool Engine::setdebug(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "on") {
        uciOptions.debug = true;
    } else if (token == "off") {
        uciOptions.debug = false;
    }
    return true;
}

bool Engine::isready(std::istringstream& iss) {
    uciHandler.ready();
    return true;
}

bool Engine::setoption(std::istringstream& iss) {
    std::string token, name, value;

    iss >> token;
    if (token != "name") {
        uciHandler.info("invalid setoption command");
        return true;
    }

    while (iss >> token && token != "value") {
        name += token;
    }

    if (name == "Threads") {
        parseSetoptionInt(iss, name, 1, MAX_THREADS, [&](int val) { threadpool.resize(val); });
    } else if (name == "Hash") {
        parseSetoptionInt(iss, name, 1, MAX_HASH_MB, [&](int val) { TT::table.resize(val); });
    } else {
        uciHandler.info("invalid setoption command");
    }
    return true;
}

bool Engine::newgame(std::istringstream& iss) {
    TT::table.age();
    threadpool.age();
    return true;
}

bool Engine::position(std::istringstream& iss) {
    std::string fen = parsePosition(iss);

    if (fen.empty()) {
        uciHandler.info("invalid position command");
        return true;
    }

    board.loadFEN(fen);

    std::string token;
    while (iss >> token) {
        auto move = getMove(token);
        if (move == NullMove) break;
        board.make(move);
    }
    return true;
}

bool Engine::go(std::istringstream& iss) {
    SearchOptions options{iss};
    options.fen   = board.toFEN();
    options.debug = uciOptions.debug;

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
    eval<Verbose>(board);
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

    for (auto& move : moves) {
        uciHandler.logOutput(move.str());
    }
    return true;
}

bool Engine::perft(std::istringstream& iss) {
    int depth = 1;

    if (iss >> depth) {
        if (0 < depth && depth < MAX_DEPTH) {
            board.perft<NodeType::Root>(depth, out);
        } else {
            uciHandler.info("invalid perft value " + std::to_string(depth));
        }
    } else {
        iss.clear();
        std::string bad;
        iss >> bad;
        uciHandler.info("invalid perft value " + bad);
    }
    return true;
}

std::string Engine::parsePosition(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "startpos") {
        iss >> token;  // Consume "moves" token
        return static_cast<std::string>(STARTFEN);
    } else if (token == "fen") {
        std::string fen;
        while (iss >> token && token != "moves") {
            fen += token + " ";
        }
        return fen;
    } else {
        return "";
    }
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

void Engine::parseSetoptionInt(
    std::istringstream& iss, const std::string& opt, int min, int max, auto&& handler) {
    int val;
    if (iss >> val && val >= min && val <= max) {
        handler(val);
    } else {
        iss.clear();
        uciHandler.info("invalid setoption command");
    }
};
