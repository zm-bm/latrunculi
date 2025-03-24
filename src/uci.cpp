#include "uci.hpp"

#include <chrono>
#include <complex>
#include <sstream>

#include "board.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "tt.hpp"

namespace UCI {

void Engine::loop() {
    uci();

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
        uci();
    } else if (token == "debug") {
        setdebug(iss);
    } else if (token == "isready") {
        std::cout << "readyok" << std::endl;
    } else if (token == "setoption") {
        std::cout << "to be implemented" << std::endl;
    } else if (token == "ucinewgame") {
        std::cout << "to be implemented" << std::endl;
    } else if (token == "position") {
        position(iss);
    } else if (token == "go") {
        go(iss);
    } else if (token == "stop") {
        threads.stopAll();
    } else if (token == "ponderhit") {
        std::cout << "to be implemented" << std::endl;
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
        std::cout << board << std::endl;
    } else if (token == "eval") {
        eval<Verbose>(board);
    }

    return true;
}

void Engine::uci() {
    std::cout << "id name Latrunculi 0.1.0" << std::endl;
    std::cout << "id author Eric VanderHelm" << std::endl;
    std::cout << "uciok" << std::endl;
}

void Engine::setdebug(std::istringstream& iss) {
    std::string token;
    iss >> token;

    if (token == "on") {
        options.debug = true;
    } else if (token == "off") {
        options.debug = false;
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

    while (iss >> token) {
        if (token == "depth") {
            iss >> token;
            options.depth = std::stoi(token);
        }
    }

    threads.startAll(board, options);
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
        std::cout << move << ": " << move.priority << std::endl;
    }
}

void printInfo(int score, int depth, SearchStats& stats, PrincipalVariation& pv) {
    using namespace std::chrono;
    auto dur = high_resolution_clock::now() - stats.startTime;
    auto sec = duration_cast<duration<double>>(dur).count();
    auto nps = (sec > 0) ? stats.totalNodes / sec : 0;

    std::cout << std::fixed;
    std::cout << "info depth " << depth;
    std::cout << " score cp " << score;
    std::cout << " time " << std::setprecision(1) << sec;
    std::cout << " nodes " << stats.totalNodes;
    std::cout << " nps " << std::setprecision(0) << nps;
    std::cout << " pv";
    for (auto& move : pv[0]) std::cout << " " << move;
    std::cout << std::endl;
}

void printDebuggingInfo(const SearchStats& stats) {
    std::cerr << "\n"
              << std::setw(5) << "Depth"
              << " | " << std::setw(9) << "Nodes"
              << " | " << std::setw(9) << "QNodes"
              << " | " << std::setw(23) << "Cutoffs (Early/Late%)"
              << " | " << std::setw(6) << "TT%"
              << " | " << std::setw(7) << "TTCut%"
              << " | " << std::setw(13) << "EBF / Cumul" << "\n";

    int maxDepth = stats.maxDepth();
    for (size_t d = 1; d < maxDepth; ++d) {
        U64 nodes   = stats.nodes[d];
        U64 prev    = stats.nodes[d - 1];
        U64 qnodes  = stats.qNodes[d];
        U64 cutoffs = stats.cutoffs[d];
        U64 early   = stats.failHighEarly[d];
        U64 late    = stats.failHighLate[d];
        U64 probes  = stats.ttProbes[d];
        U64 hits    = stats.ttHits[d];
        U64 cutTT   = stats.ttCutoffs[d];

        double ebf        = prev > 0 ? static_cast<double>(nodes) / prev : 0.0;
        double cumulative = std::pow(static_cast<double>(nodes), 1.0 / d);
        double earlyPct   = cutoffs > 0 ? 100.0 * early / cutoffs : 0.0;
        double latePct    = cutoffs > 0 ? 100.0 * late / cutoffs : 0.0;
        double ttHitPct   = probes > 0 ? 100.0 * hits / probes : 0.0;
        double ttCutPct   = hits > 0 ? 100.0 * cutTT / hits : 0.0;

        std::cerr << std::fixed;
        std::cerr << std::setw(5) << d << " | ";
        std::cerr << std::setw(9) << nodes << " | ";
        std::cerr << std::setw(9) << qnodes << " | ";
        std::cerr << std::setw(8) << cutoffs << " (";
        std::cerr << std::setw(5) << std::setprecision(1) << earlyPct << "/";
        std::cerr << std::setw(5) << std::setprecision(1) << latePct << "%) | ";
        std::cerr << std::setw(5) << std::setprecision(1) << ttHitPct << "% | ";
        std::cerr << std::setw(6) << std::setprecision(1) << ttCutPct << "% | ";
        std::cerr << std::setw(5) << std::setprecision(1) << ebf << " / ";
        std::cerr << std::setw(5) << std::setprecision(1) << cumulative << "\n";
    }
}

}  // namespace UCI