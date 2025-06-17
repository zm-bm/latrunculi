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
    if (token.empty()) return true;

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

        auto parseOptionInt = [&](const std::string& opt, int min, int max, auto&& handler) {
            int val;
            if (iss >> val) {
                if (val >= min && val <= max) {
                    handler(val);
                } else {
                    uciOutput.infoString("invalid " + opt + " value " + std::to_string(val));
                }
            } else {
                iss.clear();
                std::string bad;
                iss >> bad;
                uciOutput.infoString("invalid " + opt + " value " + bad);
            }
        };

        if (name == "Debug") {
            setdebug(iss);
        } else if (name == "Threads") {
            parseOptionInt(name, 1, MAX_THREADS, [&](int val) { threadpool.resize(val); });
        } else if (name == "Hash") {
            parseOptionInt(name, 1, MAX_HASH_MB, [&](int val) { TT::table.resize(val); });
        }
    }
}

void Engine::newgame(std::istringstream& iss) {
    TT::table.age();
    threadpool.age();
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
        uciOutput.infoString("invalid position command: " + token);
        return;
    }

    board.loadFEN(fen);

    while (iss >> token) {
        if (!tryMove(board, token)) break;
    }
}

void Engine::go(std::istringstream& iss) {
    SearchOptions options{iss};
    options.fen   = board.toFEN();
    options.debug = uciOptions.debug;

    for (const auto& w : options.warnings) {
        uciOutput.infoString("invalid " + w.name + " value " + w.value);
    }

    threadpool.startAll(options);
}

void Engine::stop(std::istringstream& iss) { threadpool.stopAll(); }

void Engine::ponderhit(std::istringstream& iss) {
    // TODO: implement pondering/ponderhit
}

void Engine::help(std::istringstream& iss) { uciOutput.help(); }

void Engine::display(std::istringstream& iss) { out << board << std::endl; }

void Engine::evaluate(std::istringstream& iss) { eval<Verbose>(board); }

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
    int depth = 1;

    if (iss >> depth) {
        if (0 < depth && depth < MAX_DEPTH) {
            board.perft<NodeType::Root>(depth, out);
        } else {
            uciOutput.infoString("invalid perft value " + std::to_string(depth));
        }
    } else {
        iss.clear();
        std::string bad;
        iss >> bad;
        uciOutput.infoString("invalid perft value " + bad);
    }
}

std::string Engine::parseFen(std::istringstream& iss) {
    std::string token, fen;
    while (iss >> token && token != "moves") {
        fen += token + " ";
    }
    return fen.substr(0, fen.size() - 1);
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
