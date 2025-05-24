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
        if (!execute(line)) break;
    }
}

bool Engine::execute(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;

    if (token == "uci") {
        uciOutput.sendIdentity();
    } else if (token == "debug") {
        setdebug(iss);
    } else if (token == "isready") {
        uciOutput.sendReady();
    } else if (token == "setoption") {
        uciOutput.toBeImplemented();
    } else if (token == "ucinewgame") {
        uciOutput.toBeImplemented();
    } else if (token == "position") {
        position(iss);
    } else if (token == "go") {
        go(iss);
    } else if (token == "stop") {
        threadpool.haltAll();
    } else if (token == "ponderhit") {
        uciOutput.toBeImplemented();
    } else if (token == "quit" || token == "exit") {
        return false;
    }

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

void Engine::setdebug(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "on") {
        uciOptions.debug = true;
    } else if (token == "off") {
        uciOptions.debug = false;
    }
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

void Engine::perft(std::istringstream& iss) {
    std::string token;
    iss >> token;

    auto val = std::stoi(token);
    board.perft<NodeType::Root>(val, out);
}

void Engine::go(std::istringstream& iss) {
    SearchOptions options{};
    options.fen   = board.toFEN();
    options.debug = uciOptions.debug;

    std::string token;
    while (iss >> token) {
        if (token == "depth") {
            iss >> token;
            options.depth = std::stoi(token);
        }
        if (token == "movetime") {
            iss >> token;
            options.movetime = std::stoi(token);
        }
    }

    threadpool.startAll(options);
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
