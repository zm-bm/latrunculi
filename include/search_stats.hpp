#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <complex>

#include "constants.hpp"

template <bool Enable = STATS_ENABLED>
struct SearchStats;

// Specialization when stats disabled
template <>
struct SearchStats<false> {
    void addNode(int) {}
    void addQNode(int) {}
    void addBetaCutoff(int, bool) {}
    void addTTProbe(int) {}
    void addTTHit(int) {}
    void addTTCutoff(int) {}
    void reset() {}

    friend std::ostream& operator<<(std::ostream& out, const SearchStats& stats) { return out; }
    SearchStats& operator+=(const SearchStats& other) { return *this; }
    SearchStats operator+(const SearchStats& other) const { return *this; }
};

// Specialization when stats enabled
template <>
struct SearchStats<true> {
    using StatsArray = std::array<U64, MAX_DEPTH>;

    StatsArray nodes{0};
    StatsArray qNodes{0};
    StatsArray cutoffs{0};
    StatsArray failHighEarly{0};
    StatsArray failHighLate{0};
    StatsArray ttProbes{0};
    StatsArray ttHits{0};
    StatsArray ttCutoffs{0};

    inline void addNode(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH) {
            nodes[ply]++;
        }
    }

    inline void addQNode(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH) {
            nodes[ply]++;
            qNodes[ply]++;
        }
    }

    inline void addBetaCutoff(const int ply, const bool early) {
        if (ply >= 0 && ply < MAX_DEPTH) {
            cutoffs[ply]++;
            if (early)
                failHighEarly[ply]++;
            else
                failHighLate[ply]++;
        }
    }

    inline void addTTProbe(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH) ttProbes[ply]++;
    }
    inline void addTTHit(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH) ttHits[ply]++;
    }
    inline void addTTCutoff(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH) ttCutoffs[ply]++;
    }

    void reset() {
        nodes.fill(0);
        qNodes.fill(0);
        cutoffs.fill(0);
        failHighEarly.fill(0);
        failHighLate.fill(0);
        ttProbes.fill(0);
        ttHits.fill(0);
        ttCutoffs.fill(0);
    }

    SearchStats& operator+=(const SearchStats& other) {
        for (size_t i = 0; i < MAX_DEPTH; ++i) {
            nodes[i]         += other.nodes[i];
            qNodes[i]        += other.qNodes[i];
            cutoffs[i]       += other.cutoffs[i];
            failHighEarly[i] += other.failHighEarly[i];
            failHighLate[i]  += other.failHighLate[i];
            ttProbes[i]      += other.ttProbes[i];
            ttHits[i]        += other.ttHits[i];
            ttCutoffs[i]     += other.ttCutoffs[i];
        }
        return *this;
    }

    SearchStats operator+(const SearchStats& other) const {
        SearchStats result  = *this;
        result             += other;
        return result;
    }

    friend std::ostream& operator<<(std::ostream& out, const SearchStats& stats) {
        out << "\n"
            << std::setw(5) << "Depth"
            << " | " << std::setw(18) << "Nodes (QNode%)"
            << " | " << std::setw(23) << "Cutoffs (Early%/Late%)"
            << " | " << std::setw(6) << "TTHit%"
            << " | " << std::setw(6) << "TTCut%"
            << " | " << std::setw(13) << "EBF / Cumul" << "\n";

        int maxDepth = MAX_DEPTH - 1;
        while (maxDepth >= 0 && stats.nodes[maxDepth] == 0) --maxDepth;

        for (size_t d = 1; d <= maxDepth; ++d) {
            U64 nodes   = stats.nodes[d];
            U64 prev    = d > 1 ? stats.nodes[d - 1] : 0;
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

        return out;
    }
};
