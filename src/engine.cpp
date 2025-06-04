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

using CommandFunc = std::function<void(std::istringstream&)>;

// Helper function for adding commands
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
            iss >> token;
            try {
                int numThreads = std::stoi(token);
                threadpool.resize(numThreads);
            } catch (const std::invalid_argument& e) {
                out << "info string invalid Threads value: " << token << std::endl;
            }
        } else if (name == "Hash") {
            iss >> token;
            try {
                size_t hash = std::stoul(token);
                // TT::resize(hash);
            } catch (const std::invalid_argument& e) {
                out << "info string invalid Hash value" << token << std::endl;
            }
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
        iss >> token;
    } else if (token == "fen") {
        while (iss >> token && token != "moves") {
            fen += token + " ";
        }
    } else {
        return;
    }

    board.loadFEN(fen);

    if (token == "moves") {
        while (iss >> token) {
            MoveGenerator<GenType::All> moves{board};

            for (auto& move : moves) {
                std::ostringstream oss;
                oss << move;

                if (oss.str() == token && board.isLegalMove(move)) {
                    board.make(move);
                }
            }
        }
    }
}

void Engine::go(std::istringstream& iss) {
    SearchOptions options{};
    options.fen   = board.toFEN();
    options.debug = uciOptions.debug;

    std::string token;
    while (iss >> token) {
        if (token == "depth") {
            iss >> token;
            try {
                options.depth = std::stoi(token);
            } catch (const std::invalid_argument& e) {
                out << "info string invalid depth value: " << token << std::endl;
            }
        }
        if (token == "movetime") {
            iss >> token;
            try {
                options.movetime = std::stoi(token);
            } catch (const std::invalid_argument& e) {
                std::cerr << "info string invalid movetime value: " << token << std::endl;
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

void Engine::display(std::istringstream& iss) { out << board.toFEN() << std::endl; }

void Engine::evaluate(std::istringstream& iss) { auto score = eval<Verbose>(board); }

void Engine::move(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "undo") {
        board.unmake();
    } else {
        MoveGenerator<GenType::All> moves{board};

        for (auto& move : moves) {
            std::ostringstream oss;
            oss << move;

            if (oss.str() == token && board.isLegalMove(move)) {
                board.make(move);
            }
        }
    }
}

void Engine::moves(std::istringstream& iss) {
    MoveGenerator<GenType::All> moves{board};

    for (auto& move : moves) {
        out << move << '\n';
    }
}

void Engine::perft(std::istringstream& iss) {
    std::string token;
    iss >> token;
    try {
        auto depth = std::stoi(token);
        board.perft<NodeType::Root>(depth, out);
    } catch (const std::invalid_argument& e) {
        out << "info string invalid perft value: " << token << std::endl;
    }
}
