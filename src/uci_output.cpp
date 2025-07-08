#include "uci_output.hpp"

#include <iomanip>

#include "base.hpp"
#include "board.hpp"
#include "constants.hpp"

std::string UCIInfo::formatScore() const {
    std::ostringstream oss;
    if (isMateScore(score)) {
        int mateInMoves = (mateDistance(score) + 1) / 2;
        oss << "mate " << (score > 0 ? "" : "-") << mateInMoves;
    } else {
        oss << "cp " << score;
    }
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const UCIInfo& info) {
    auto timeCount = info.time.count();

    os << std::fixed;
    os << "info depth " << info.depth;
    os << " score " << info.formatScore();
    os << " time " << timeCount;
    os << " nodes " << info.nodes;
    os << " nps " << (timeCount > 0 ? (info.nodes * 1000 / timeCount) : 0);
    os << " pv " << info.pv;
    return os;
}

void UCIOutput::identify() const {
    out << "id name Latrunculi " << VERSION << "\n";
    out << "id author Eric VanderHelm\n\n";
    out << "option name Threads type spin default " << DEFAULT_THREADS << " min 1 max "
        << MAX_THREADS << "\n";
    out << "option name Hash type spin default " << DEFAULT_HASH_MB << " min 1 max " << MAX_HASH_MB
        << "\n";
    out << "uciok" << std::endl;
}

void UCIOutput::ready() const { out << "readyok" << std::endl; }

void UCIOutput::bestmove(std::string move) const { out << "bestmove " << move << std::endl; }

void UCIOutput::help() const {
    out << "Available commands:\n"
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

void UCIOutput::info(const UCIInfo& info) { out << info << std::endl; }

void UCIOutput::info(const std::string& str) const { out << "info string " << str << std::endl; }

void UCIOutput::stats(SearchStats<> stats) const { out << stats << std::endl; }
