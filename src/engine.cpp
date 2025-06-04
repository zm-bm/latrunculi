#include "engine.hpp"

#include <chrono>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <variant>

#include "board.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "search_options.hpp"
#include "thread.hpp"
#include "tt.hpp"

// Sentinel for invalid integers
constexpr int INVALID_INT = std::numeric_limits<int>::min();

// Helpers for adding commands
using CommandFunc = std::function<void(std::istringstream&)>;
CommandFunc makeCommand(Engine* engine, void (Engine::*func)(std::istringstream&)) {
    return [engine, func](std::istringstream& iss) { (engine->*func)(iss); };
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

    bool shouldExit = false;

    static const std::unordered_map<std::string, CommandFunc> commands = {
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
        {"quit", [&shouldExit](std::istringstream&) { shouldExit = true; }},
        {"exit", [&shouldExit](std::istringstream&) { shouldExit = true; }},
        // Non-UCI utility commands
        {"help", makeCommand(this, &Engine::help)},
        {"d", makeCommand(this, &Engine::display)},
        {"eval", makeCommand(this, &Engine::evaluate)},
        {"move", makeCommand(this, &Engine::move)},
        {"moves", makeCommand(this, &Engine::moves)},
        {"perft", makeCommand(this, &Engine::perft)},
    };

    auto it = commands.find(token);
    if (it != commands.end()) {
        it->second(iss);
    } else {
        out << "Unknown command: '" << token << "'. Type help for a list of commands." << std::endl;
    }

    return !shouldExit;
}

void Engine::uci(std::istringstream& iss) { uciOutput.identify(); }

void Engine::setdebug(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "on") {
        uciOptions.debug = true;
    } else if (token == "off") {
        uciOptions.debug = false;
    }
}

void Engine::isready(std::istringstream& iss) { uciOutput.ready(); }

void Engine::setoption(std::istringstream& iss) {
    std::string token, name, value;
    iss >> token;

    if (token == "name") {
        while (iss >> token && token != "value") {
            name += token;
        }

        if (name == "Debug") {
            setdebug(iss);
        } else if (name == "Threads") {
            int value = parseInt(iss, "Threads", 1, 64);
            if (value == INVALID_INT) return;

            threadpool.resize(value);
        } else if (name == "Hash") {
            int value = parseInt(iss, "Hash", 1, 1024);
            if (value == INVALID_INT) return;

            // TT::resize(hash);
        }
    }
}

void Engine::newgame(std::istringstream& iss) {
    // tt.clear();
    // threadpool.resetHeuristics();
}

void Engine::position(std::istringstream& iss) {
    std::string token, fen;
    iss >> token;

    if (token == "startpos") {
        fen = STARTFEN;
        iss >> token;  // Consume "moves" token
    } else if (token == "fen") {
        fen = parseFen(iss);
    } else {
        out << "info string invalid fen: " << token << std::endl;
        return;
    }

    board.loadFEN(fen);
    loadMoves(iss);
}

void Engine::go(std::istringstream& iss) {
    SearchOptions options{};
    options.fen   = board.toFEN();
    options.debug = uciOptions.debug;

    std::string token;
    while (iss >> token) {
        if (token == "depth") {
            int depth = parseInt(iss, "depth", 1, MAX_DEPTH);
            if (depth != INVALID_INT) {
                options.depth = depth;
            }
        }
        if (token == "movetime") {
            int movetime = parseInt(iss, "movetime", 1, INT32_MAX);
            if (movetime != INVALID_INT) {
                options.movetime = movetime;
            }
        }
    }

    threadpool.startAll(options);
}

void Engine::stop(std::istringstream& iss) { threadpool.stopAll(); }

void Engine::ponderhit(std::istringstream& iss) {
    // TODO: implement pondering/ponderhit
}

void Engine::help(std::istringstream& iss) { uciOutput.help(); }

void Engine::display(std::istringstream& iss) { out << board << std::endl; }

void Engine::evaluate(std::istringstream& iss) { auto score = eval<Verbose>(board); }

void Engine::move(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "undo") {
        board.unmake();
    } else {
        tryMove(board, token);
    }
}

void Engine::moves(std::istringstream& iss) {
    MoveGenerator<GenType::All> moves{board};

    for (auto& move : moves) {
        out << move << '\n';
    }
}

void Engine::perft(std::istringstream& iss) {
    int depth = parseInt(iss, "perft", 1, MAX_DEPTH);

    if (depth != INVALID_INT) {
        board.perft<NodeType::Root>(depth, out);
    }
}

std::string Engine::parseFen(std::istringstream& iss) {
    std::string token, fen;
    while (iss >> token && token != "moves") {
        fen += token + " ";
    }
    std::cout << fen << std::endl;
    return fen.substr(0, fen.size() - 1);
}

void Engine::loadMoves(std::istringstream& iss) {
    std::string token;
    while (iss >> token) {
        if (!tryMove(board, token)) break;
    }
}

bool Engine::tryMove(Board& board, const std::string& token) {
    MoveGenerator<GenType::All> moves{board};
    for (auto& move : moves) {
        if (move.str() == token && board.isLegalMove(move)) {
            board.make(move);
            return true;
        }
    }
    return false;
}

int Engine::parseInt(std::istringstream& iss, std::string errorMsg, int min, int max) {
    std::string token;
    iss >> token;
    try {
        int value = std::stoi(token);

        if (value < min) {
            out << "info string invalid " << errorMsg << " value: " << value << ", using " << min
                << std::endl;
            return min;
        }
        if (value > max) {
            out << "info string invalid " << errorMsg << " value: " << value << ", using " << max
                << std::endl;
            return max;
        }

        return value;
    } catch (const std::invalid_argument& e) {
        out << "info string invalid " << errorMsg << " value: " << token << std::endl;
        return INVALID_INT;
    }
}
