#include "uci.hpp"

#include <sstream>

#include "board.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "tt.hpp"

namespace UCI {

Engine::Engine(std::istream& is, std::ostream& os)
    : board(STARTFEN), threads(1), debug(false), istream(is), ostream(os) {}

void Engine::loop() {
    std::string line;
    uci();

    while (std::getline(istream, line)) {
        if (!execute(line)) break;
    }
}

bool Engine::execute(const std::string& line) {
    std::istringstream iss(line);
    std::string token;
    iss >> token;

    if (token == "uci") {
        uci();
    } else if (token == "debug") {
        setdebug(iss);
    } else if (token == "isready") {
        ostream << "readyok" << std::endl;
    } else if (token == "setoption") {
        ostream << "to be implemented" << std::endl;
    } else if (token == "ucinewgame") {
        ostream << "to be implemented" << std::endl;
    } else if (token == "position") {
        position(iss);
    } else if (token == "go") {
        go(iss);
    } else if (token == "stop") {
        threads.stopAll();
    } else if (token == "ponderhit") {
        ostream << "to be implemented" << std::endl;
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
        ostream << board << std::endl;
    } else if (token == "eval") {
        eval<Verbose>(board);
    }

    return true;
}

void Engine::uci() {
    ostream << "id name Latrunculi 0.1.0" << std::endl;
    ostream << "id author Eric VanderHelm" << std::endl;
    ostream << "uciok" << std::endl;
}

void Engine::setdebug(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "on") {
        debug = true;
    } else if (token == "off") {
        debug = false;
    }
}

void Engine::position(std::istringstream& iss) {
    std::string token, fen;
    iss >> token;

    if (token == "startpos") {
        fen = STARTFEN;
    } else if (token == "fen") {
        while (iss >> token && token != "moves") {
            fen += token + " ";
        }
    } else {
        return;
    }

    board = Board(fen);
}

void Engine::perft(std::istringstream& iss) {
    std::string token;
    iss >> token;

    auto val = std::stoi(token);
    Search::perft<Search::NodeType::Root>(val, board);
}

void Engine::go(std::istringstream& iss) {
    std::string token;
    int depth = 12;

    while (iss >> token) {
        if (token == "depth") {
            iss >> token;
            depth = std::stoi(token);
        }
    }

    threads.startAll(board, depth);
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
        ostream << move << ": " << move.priority << std::endl;
    }
}

}  // namespace UCI