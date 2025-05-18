#include "uci_output.hpp"

#include <complex>
#include <iomanip>

#include "board.hpp"
#include "constants.hpp"

void UCIOutput::sendIdentity() const {
    out << "id name Latrunculi " << VERSION << std::endl;
    out << "id author Eric VanderHelm" << std::endl;
    out << "uciok" << std::endl;
}

void UCIOutput::sendReady() const { out << "readyok" << std::endl; }

void UCIOutput::sendBestmove(std::string move) const { out << "bestmove " << move << std::endl; }

void UCIOutput::toBeImplemented() const { out << "to be implemented" << std::endl; }

void UCIOutput::sendInfo(int score, int depth, U64 nodes, Milliseconds ms, std::string pv) {
    if (score == lastScore && pv == lastPV) return;
    lastScore = score;
    lastPV    = pv;

    auto time = ms.count();

    out << std::fixed;
    out << "info depth " << depth;
    out << " score " << formatScore(score);
    out << " time " << time;
    out << " nodes " << nodes;
    out << " nps " << (time > 0 ? (nodes * 1000 / time) : 0);
    out << " pv " << pv;
    out << std::endl;
}

void UCIOutput::sendStats(SearchStats stats) const {
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

std::string UCIOutput::formatScore(int score) {
    std::ostringstream oss;
    if (isMateScore(score)) {
        int mateInMoves = (mateDistance(score) + 1) / 2;
        oss << "mate " << (score > 0 ? "" : "-") << mateInMoves;
    } else {
        oss << "cp " << score;
    }
    return oss.str();
}
