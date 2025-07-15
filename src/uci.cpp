#include "uci.hpp"

#include <iomanip>

#include "base.hpp"
#include "board.hpp"
#include "constants.hpp"

// --------------------------
// UCIBestLine implementation
// --------------------------

UCIBestLine::UCIBestLine(int score, int depth, U64 nodes, Milliseconds time, std::string pv)
    : score(score), depth(depth), nodes(nodes), time(time), pv(std::move(pv)) {}

std::string UCIBestLine::formatScore() const {
    std::ostringstream oss;
    if (isMateScore(score)) {
        int mateInMoves = (mateDistance(score) + 1) / 2;
        oss << "mate " << (score > 0 ? "" : "-") << mateInMoves;
    } else {
        oss << "cp " << score;
    }
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const UCIBestLine& info) {
    auto timeCount = info.time.count();

    os << std::fixed;
    os << "depth " << info.depth;
    os << " score " << info.formatScore();
    os << " time " << timeCount;
    os << " nodes " << info.nodes;
    os << " nps " << (timeCount > 0 ? (info.nodes * 1000 / timeCount) : 0);
    os << " pv " << info.pv;
    return os;
}

// --------------------------
// UCIProtocolHandler implementation
// --------------------------

void UCIProtocolHandler::help() const {
    err << "Available commands:\n"
        << "  uci           - Show engine identity and supported options\n"
        << "  isready       - Check if the engine is ready\n"
        << "  setoption     - Set engine options\n"
        << "  ucinewgame    - Start a new game\n"
        << "  position      - Set up the board position\n"
        << "  go            - Start searching for the best move\n"
        << "  stop          - Stop the search\n"
        << "  ponderhit     - Handle ponder hit\n"
        << "  quit          - Exit the engine\n"
        << "  perft <depth> - Run perft for the given depth\n"
        << "  move <move>   - Make a move on the board\n"
        << "  moves         - Show all legal moves\n"
        << "  d             - Display the current board position\n"
        << "  eval          - Evaluate the current position\n"
        << "For UCI protocol details, see: https://www.wbec-ridderkerk.nl/html/UCIProtocol.html\n"
        << std::endl;
}

void UCIProtocolHandler::identify(const UCIConfig& config) const {
    out << "id name Latrunculi " << VERSION << "\n";
    out << "id author Eric VanderHelm\n";
    out << config;
    out << "uciok" << std::endl;
}

void UCIProtocolHandler::ready() const { out << "readyok" << std::endl; }

void UCIProtocolHandler::bestmove(std::string move) const {
    out << "bestmove " << move << std::endl;
}

void UCIProtocolHandler::info(const UCIBestLine& info) const {
    out << "info " << info << std::endl;
}

void UCIProtocolHandler::info(const std::string& str) const {
    out << "info string " << str << std::endl;
}
