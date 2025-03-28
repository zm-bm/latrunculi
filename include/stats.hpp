#include <array>
#include <chrono>

#include "constants.hpp"

struct SearchStats {
    using TimePoint     = std::chrono::high_resolution_clock::time_point;
    TimePoint startTime = std::chrono::high_resolution_clock::now();
    U64 totalNodes      = 0;
    bool debug          = true;

    std::array<U64, MAX_DEPTH> nodes{};
    std::array<U64, MAX_DEPTH> qNodes{};
    std::array<U64, MAX_DEPTH> cutoffs{};
    std::array<U64, MAX_DEPTH> failHighEarly{};
    std::array<U64, MAX_DEPTH> failHighLate{};
    std::array<U64, MAX_DEPTH> ttProbes{};
    std::array<U64, MAX_DEPTH> ttHits{};
    std::array<U64, MAX_DEPTH> ttCutoffs{};

    void addNode(int ply) {
        totalNodes++;
        if (debug) nodes[ply]++;
    }

    void addQNode(int ply) {
        totalNodes++;
        if (debug) {
            nodes[ply]++;
            qNodes[ply]++;
        }
    }

    void addBetaCutoff(int ply, bool early) {
        if (debug) {
            cutoffs[ply]++;
            if (early)
                failHighEarly[ply]++;
            else
                failHighLate[ply]++;
        }
    }

    void addTTProbe(int ply) {
        if (debug) ttProbes[ply]++;
    }
    void addTTHit(int ply) {
        if (debug) ttHits[ply]++;
    }
    void addTTCutoff(int ply) {
        if (debug) ttCutoffs[ply]++;
    }

    int maxDepth() const {
        for (int d = MAX_DEPTH - 1; d >= 0; --d) {
            if (nodes[d] > 0 || qNodes[d] > 0) {
                return d;
            }
        }
        return 0;
    }

    void reset() { *this = {}; }

    void resetDepthStats() {
        nodes         = {};
        qNodes        = {};
        cutoffs       = {};
        failHighEarly = {};
        failHighLate  = {};
        ttProbes      = {};
        ttHits        = {};
        ttCutoffs     = {};
    }
};
