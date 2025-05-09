#pragma once

#include <array>
#include <atomic>
#include <chrono>

#include "constants.hpp"

constexpr U64 NodeInterval = 1000;

struct SearchStats {
    bool debug = true;

    U64 totalNodes{0};

    std::array<U64, MAX_DEPTH> nodes{0};
    std::array<U64, MAX_DEPTH> qNodes{0};
    std::array<U64, MAX_DEPTH> cutoffs{0};
    std::array<U64, MAX_DEPTH> failHighEarly{0};
    std::array<U64, MAX_DEPTH> failHighLate{0};
    std::array<U64, MAX_DEPTH> ttProbes{0};
    std::array<U64, MAX_DEPTH> ttHits{0};
    std::array<U64, MAX_DEPTH> ttCutoffs{0};

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

    void reset() {
        resetDepthStats();
        totalNodes = 0;
    }

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
