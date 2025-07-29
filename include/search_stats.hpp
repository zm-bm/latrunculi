#pragma once

#include <array>

#include "defs.hpp"

template <bool Enable = SEARCH_STATS>
struct SearchStats;

// stats disabled specialization
template <>
struct SearchStats<false> {
    void node(int) {}
    void qnode(int) {}
    void beta_cutoff(int, bool) {}
    void tt_probe(int) {}
    void tt_hit(int) {}
    void tt_cutoff(int) {}
    void reset() {}

    friend std::ostream& operator<<(std::ostream& out, const SearchStats& stats) { return out; }
    SearchStats&         operator+=(const SearchStats& other) { return *this; }
    SearchStats          operator+(const SearchStats& other) const { return *this; }
};

// stats enabled specialization
template <>
struct SearchStats<true> {
    using StatsArray = std::array<uint64_t, MAX_DEPTH>;

    StatsArray nodes{0};
    StatsArray qnodes{0};
    StatsArray cutoffs{0};
    StatsArray fail_high_early{0};
    StatsArray fail_high_late{0};
    StatsArray tt_probes{0};
    StatsArray tt_hits{0};
    StatsArray tt_cutoffs{0};

    void node(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH) {
            nodes[ply]++;
        }
    }

    void qnode(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH) {
            nodes[ply]++;
            qnodes[ply]++;
        }
    }

    void beta_cutoff(const int ply, const bool early) {
        if (ply >= 0 && ply < MAX_DEPTH) {
            cutoffs[ply]++;
            if (early)
                fail_high_early[ply]++;
            else
                fail_high_late[ply]++;
        }
    }

    void tt_probe(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH)
            tt_probes[ply]++;
    }

    void tt_hit(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH)
            tt_hits[ply]++;
    }

    void tt_cutoff(const int ply) {
        if (ply >= 0 && ply < MAX_DEPTH)
            tt_cutoffs[ply]++;
    }

    void reset() {
        nodes.fill(0);
        qnodes.fill(0);
        cutoffs.fill(0);
        fail_high_early.fill(0);
        fail_high_late.fill(0);
        tt_probes.fill(0);
        tt_hits.fill(0);
        tt_cutoffs.fill(0);
    }

    SearchStats& operator+=(const SearchStats& other) {
        for (size_t i = 0; i < MAX_DEPTH; ++i) {
            nodes[i]           += other.nodes[i];
            qnodes[i]          += other.qnodes[i];
            cutoffs[i]         += other.cutoffs[i];
            fail_high_early[i] += other.fail_high_early[i];
            fail_high_late[i]  += other.fail_high_late[i];
            tt_probes[i]       += other.tt_probes[i];
            tt_hits[i]         += other.tt_hits[i];
            tt_cutoffs[i]      += other.tt_cutoffs[i];
        }
        return *this;
    }

    SearchStats operator+(const SearchStats& other) const {
        SearchStats result  = *this;
        result             += other;
        return result;
    }
};

template <>
struct std::formatter<SearchStats<false>> : std::formatter<std::string_view> {
    auto format(const SearchStats<false>& stats, std::format_context& ctx) const {
        return ctx.out();
    }
};

template <>
struct std::formatter<SearchStats<true>> : std::formatter<std::string_view> {
    auto format(const SearchStats<true>& stats, std::format_context& ctx) const {
        auto out = ctx.out();

        out = std::format_to(out,
                             "\n{:>5} | {:>18} | {:>23} | {:>6} | {:>6} | {:>13}\n",
                             "Depth",
                             "Nodes (QNode%)",
                             "Cutoffs (Early%/Late%)",
                             "TTHit%",
                             "TTCut%",
                             "EBF / Cumul");

        int maxDepth = MAX_DEPTH - 1;
        while (maxDepth > 0 && stats.nodes[maxDepth] == 0)
            --maxDepth;

        for (size_t d = 1; d <= static_cast<size_t>(maxDepth); ++d) {
            uint64_t nodes     = stats.nodes[d];
            uint64_t prev      = d > 1 ? stats.nodes[d - 1] : 0;
            uint64_t qnodes    = stats.qnodes[d];
            uint64_t cutoffs   = stats.cutoffs[d];
            uint64_t early     = stats.fail_high_early[d];
            uint64_t late      = stats.fail_high_late[d];
            uint64_t tt_probes = stats.tt_probes[d];
            uint64_t tt_hits   = stats.tt_hits[d];
            uint64_t tt_cuts   = stats.tt_cutoffs[d];

            double qnode_pct  = nodes > 0 ? 100.0 * qnodes / nodes : 0.0;
            double early_pct  = cutoffs > 0 ? 100.0 * early / cutoffs : 0.0;
            double later_pct  = cutoffs > 0 ? 100.0 * late / cutoffs : 0.0;
            double tt_hit_pct = tt_probes > 0 ? 100.0 * tt_hits / tt_probes : 0.0;
            double tt_cut_pct = tt_hits > 0 ? 100.0 * tt_cuts / tt_hits : 0.0;
            double ebf        = prev > 0 ? static_cast<double>(nodes) / prev : 0.0;
            double cumulative = std::pow(static_cast<double>(nodes), 1.0 / d);

            out = std::format_to(out, "{:>5} | ", d);
            out = std::format_to(out, "{:9} ({:5.1f}%) | ", nodes, qnode_pct);
            out = std::format_to(out, "{:8} ({:5.1f}/{:5.1f}%) | ", cutoffs, early_pct, later_pct);
            out = std::format_to(out, "{:5.1f}% | ", tt_hit_pct);
            out = std::format_to(out, "{:5.1f}% | ", tt_cut_pct);
            out = std::format_to(out, "{:5.1f} / {:5.1f}\n", ebf, cumulative);
        }

        return out;
    }
};
