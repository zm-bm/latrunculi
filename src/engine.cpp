#include "engine.hpp"

#include <chrono>
#include <complex>
#include <sstream>

#include "board.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "searchoptions.hpp"
#include "thread.hpp"
#include "tt.hpp"

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
        threadpool.stopAll();
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
        iss >> token;
    } else if (token == "fen") {
        while (iss >> token && token != "moves") {
            fen += token + " ";
        }
    } else {
        return;
    }

    board = Board(fen);

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
    board.perft<NodeType::Root>(val);
}

void Engine::go(std::istringstream& iss) {
    std::string token;
    SearchOptions options{};
    options.debug = debug;

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

    threadpool.startAll(board, options);
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

// output

void Engine::uci() {
    out << "id name Latrunculi 0.1.0" << std::endl;
    out << "id author Eric VanderHelm" << std::endl;
    out << "uciok" << std::endl;
}

void Engine::bestmove(Move move) { out << "bestmove " << move << std::endl; }

std::string formatScore(int score) {
    std::ostringstream oss;
    if (isMateScore(score)) {
        int mateInMoves = (mateDistance(score) + 1) / 2;
        oss << "mate " << (score > 0 ? "" : "-") << mateInMoves;
    } else {
        oss << "cp " << score;
    }
    return oss.str();
}

void Engine::info(int score, int depth, Milliseconds searchtime, PrincipalVariation& pv) {
    Statistics stats = threadpool.aggregateStats();
    auto time        = searchtime.count();

    out << std::fixed;
    out << "info depth " << depth;
    out << " score " << formatScore(score);
    out << " time " << time;
    out << " nodes " << stats.totalNodes;
    out << " nps " << (time > 0 ? (stats.totalNodes * 1000 / time) : 0);
    out << " pv";
    for (auto& move : pv[0]) out << " " << move;
    out << std::endl;
}

void Engine::searchStats() {
    Statistics stats = threadpool.aggregateStats();

    out << "\n"
        << std::setw(5) << "Depth"
        << " | " << std::setw(18) << "Nodes (QNode%)"
        << " | " << std::setw(23) << "Cutoffs (Early%/Late%)"
        << " | " << std::setw(6) << "TTHit%"
        << " | " << std::setw(6) << "TTCut%"
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

        double quiesPct   = nodes > 0 ? 100.0 * qnodes / nodes : 0.0;
        double earlyPct   = cutoffs > 0 ? 100.0 * early / cutoffs : 0.0;
        double latePct    = cutoffs > 0 ? 100.0 * late / cutoffs : 0.0;
        double ttHitPct   = probes > 0 ? 100.0 * hits / probes : 0.0;
        double ttCutPct   = hits > 0 ? 100.0 * cutTT / hits : 0.0;
        double ebf        = prev > 0 ? static_cast<double>(nodes) / prev : 0.0;
        double cumulative = std::pow(static_cast<double>(nodes), 1.0 / d);

        out << std::fixed;
        out << std::setw(5) << d << " | ";
        out << std::setw(9) << nodes << " (";
        out << std::setw(5) << std::setprecision(1) << quiesPct << "%) | ";
        out << std::setw(8) << cutoffs << " (";
        out << std::setw(5) << std::setprecision(1) << earlyPct << "/";
        out << std::setw(5) << std::setprecision(1) << latePct << "%) | ";
        out << std::setw(5) << std::setprecision(1) << ttHitPct << "% | ";
        out << std::setw(5) << std::setprecision(1) << ttCutPct << "% | ";
        out << std::setw(5) << std::setprecision(1) << ebf << " / ";
        out << std::setw(5) << std::setprecision(1) << cumulative << "\n";
    }
}
