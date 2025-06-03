#include "engine.hpp"

#include <chrono>
#include <sstream>

#include "board.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "search_options.hpp"
#include "thread.hpp"
#include "tt.hpp"

void Engine::loop() {
    uciOutput.sendIdentity();

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

    // UCI commands
    if (token == "uci") {
        uciOutput.sendIdentity();
    } else if (token == "debug") {
        setdebug(iss);
    } else if (token == "isready") {
        uciOutput.sendReady();
    } else if (token == "setoption") {
        setoption(iss);
    } else if (token == "ucinewgame") {
        // TT.clear
        // ThreadPool::resetHeuristics
    } else if (token == "position") {
        position(iss);
    } else if (token == "go") {
        go(iss);
    } else if (token == "stop") {
        threadpool.stopAll();
    } else if (token == "quit" || token == "exit") {
        return false;
    }

    // Non-UCI commands
    if (token == "perft") {
        perft(iss);
    } else if (token == "move") {
        move(iss);
    } else if (token == "moves") {
        moves();
    } else if (token == "d") {
        out << board << std::endl;
    } else if (token == "eval") {
        eval<Verbose>(board);
    }

    return true;
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

void Engine::setdebug(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "on") {
        uciOptions.debug = true;
    } else if (token == "off") {
        uciOptions.debug = false;
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

void Engine::moves() {
    MoveGenerator<GenType::All> moves{board};

    for (auto& move : moves) {
        out << move << '\n';
    }
}
