#pragma once

#include <array>
#include <atomic>
#include <chrono>

#include "constants.hpp"

constexpr U64 NodeInterval = 8196;

struct SearchStats {
    bool debug = true;

    std::atomic<U64> totalNodes{0};
    using TimePoint     = std::chrono::high_resolution_clock::time_point;
    TimePoint startTime = std::chrono::high_resolution_clock::now();

    std::array<U64, MAX_DEPTH> nodes{};
    std::array<U64, MAX_DEPTH> qNodes{};
    std::array<U64, MAX_DEPTH> cutoffs{};
    std::array<U64, MAX_DEPTH> failHighEarly{};
    std::array<U64, MAX_DEPTH> failHighLate{};
    std::array<U64, MAX_DEPTH> ttProbes{};
    std::array<U64, MAX_DEPTH> ttHits{};
    std::array<U64, MAX_DEPTH> ttCutoffs{};

    bool checkTime(int movetime) const {
        return totalNodes % NodeInterval == 0 && elapsedTime() > movetime;
    }

    int elapsedTime() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    }

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
        totalNodes.store(0);
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
