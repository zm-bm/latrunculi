#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <ostream>
#include <string_view>

#include "defs.hpp"

template <bool Enable = SEARCH_STATS>
struct SearchStats;

// stats disabled specialization
template <>
struct SearchStats<false> {
    void node(int) {}
    void qnode(int) {}
    void beta_cutoff(int, int) {}
    void pvs_research(int) {}
    void aspiration_fail_low() {}
    void aspiration_fail_high() {}
    void aspiration_research() {}
    void main_tt_probe(int) {}
    void main_tt_hit(int) {}
    void main_tt_cutoff(int) {}
    void q_tt_probe(int) {}
    void q_tt_hit(int) {}
    void q_tt_cutoff(int) {}
    void staged_node(int) {}
    void staged_generation(int, bool, bool) {}
    void staged_skip_quiet(int) {}
    void staged_cutoff_before_quiet(int) {}
    void staged_cutoff_quiet(int) {}
    void staged_cutoff_bad_noisy(int) {}
    void reset() {}

    friend std::ostream& operator<<(std::ostream& out, const SearchStats& stats) { return out; }
    SearchStats&         operator+=(const SearchStats& other) { return *this; }
    SearchStats          operator+(const SearchStats& other) const { return *this; }
};

// stats enabled specialization
template <>
struct SearchStats<true> {
    using StatsArray = std::array<uint64_t, MAX_SEARCH_PLY>;

    StatsArray nodes{0};
    StatsArray qnodes{0};

    StatsArray cutoffs{0};
    StatsArray fail_high_early{0};
    StatsArray fail_high_late{0};
    StatsArray cutoff_index_sum{0};
    StatsArray cutoff_index_1{0};
    StatsArray cutoff_index_2{0};
    StatsArray cutoff_index_3_4{0};
    StatsArray cutoff_index_5_plus{0};

    StatsArray pvs_researches{0};

    StatsArray main_tt_probes{0};
    StatsArray main_tt_hits{0};
    StatsArray main_tt_cutoffs{0};
    StatsArray q_tt_probes{0};
    StatsArray q_tt_hits{0};
    StatsArray q_tt_cutoffs{0};
    StatsArray staged_nodes{0};
    StatsArray staged_noisy_generated_nodes{0};
    StatsArray staged_quiet_generated_nodes{0};
    StatsArray staged_skip_quiet_nodes{0};
    StatsArray staged_cutoff_before_quiet_nodes{0};
    StatsArray staged_cutoff_quiet_nodes{0};
    StatsArray staged_cutoff_bad_noisy_nodes{0};

    uint64_t aspiration_fail_lows{0};
    uint64_t aspiration_fail_highs{0};
    uint64_t aspiration_researches{0};

    void node(const int ply) {
        if (valid_ply(ply))
            nodes[ply]++;
    }

    void qnode(const int ply) {
        if (valid_ply(ply)) {
            nodes[ply]++;
            qnodes[ply]++;
        }
    }

    void beta_cutoff(const int ply, const int move_index) {
        if (!valid_ply(ply) || move_index <= 0)
            return;

        cutoffs[ply]++;
        cutoff_index_sum[ply] += static_cast<uint64_t>(move_index);

        if (move_index == 1) {
            fail_high_early[ply]++;
            cutoff_index_1[ply]++;
        } else {
            fail_high_late[ply]++;
            if (move_index == 2)
                cutoff_index_2[ply]++;
            else if (move_index <= 4)
                cutoff_index_3_4[ply]++;
            else
                cutoff_index_5_plus[ply]++;
        }
    }

    void pvs_research(const int ply) {
        if (valid_ply(ply))
            pvs_researches[ply]++;
    }

    void aspiration_fail_low() { aspiration_fail_lows++; }
    void aspiration_fail_high() { aspiration_fail_highs++; }
    void aspiration_research() { aspiration_researches++; }

    void main_tt_probe(const int ply) {
        if (valid_ply(ply))
            main_tt_probes[ply]++;
    }

    void main_tt_hit(const int ply) {
        if (valid_ply(ply))
            main_tt_hits[ply]++;
    }

    void main_tt_cutoff(const int ply) {
        if (valid_ply(ply))
            main_tt_cutoffs[ply]++;
    }

    void q_tt_probe(const int ply) {
        if (valid_ply(ply))
            q_tt_probes[ply]++;
    }

    void q_tt_hit(const int ply) {
        if (valid_ply(ply))
            q_tt_hits[ply]++;
    }

    void q_tt_cutoff(const int ply) {
        if (valid_ply(ply))
            q_tt_cutoffs[ply]++;
    }

    void staged_node(const int ply) {
        if (!valid_ply(ply))
            return;

        staged_nodes[ply]++;
    }

    void staged_generation(const int ply, const bool noisy_generated, const bool quiet_generated) {
        if (!valid_ply(ply))
            return;

        if (noisy_generated)
            staged_noisy_generated_nodes[ply]++;
        if (quiet_generated)
            staged_quiet_generated_nodes[ply]++;
    }

    void staged_skip_quiet(const int ply) {
        if (valid_ply(ply))
            staged_skip_quiet_nodes[ply]++;
    }

    void staged_cutoff_before_quiet(const int ply) {
        if (valid_ply(ply))
            staged_cutoff_before_quiet_nodes[ply]++;
    }

    void staged_cutoff_quiet(const int ply) {
        if (valid_ply(ply))
            staged_cutoff_quiet_nodes[ply]++;
    }

    void staged_cutoff_bad_noisy(const int ply) {
        if (valid_ply(ply))
            staged_cutoff_bad_noisy_nodes[ply]++;
    }

    void reset() {
        nodes.fill(0);
        qnodes.fill(0);
        cutoffs.fill(0);
        fail_high_early.fill(0);
        fail_high_late.fill(0);
        cutoff_index_sum.fill(0);
        cutoff_index_1.fill(0);
        cutoff_index_2.fill(0);
        cutoff_index_3_4.fill(0);
        cutoff_index_5_plus.fill(0);
        pvs_researches.fill(0);
        main_tt_probes.fill(0);
        main_tt_hits.fill(0);
        main_tt_cutoffs.fill(0);
        q_tt_probes.fill(0);
        q_tt_hits.fill(0);
        q_tt_cutoffs.fill(0);
        staged_nodes.fill(0);
        staged_noisy_generated_nodes.fill(0);
        staged_quiet_generated_nodes.fill(0);
        staged_skip_quiet_nodes.fill(0);
        staged_cutoff_before_quiet_nodes.fill(0);
        staged_cutoff_quiet_nodes.fill(0);
        staged_cutoff_bad_noisy_nodes.fill(0);
        aspiration_fail_lows  = 0;
        aspiration_fail_highs = 0;
        aspiration_researches = 0;
    }

    SearchStats& operator+=(const SearchStats& other) {
        for (size_t i = 0; i < MAX_SEARCH_PLY; ++i) {
            nodes[i]               += other.nodes[i];
            qnodes[i]              += other.qnodes[i];
            cutoffs[i]             += other.cutoffs[i];
            fail_high_early[i]     += other.fail_high_early[i];
            fail_high_late[i]      += other.fail_high_late[i];
            cutoff_index_sum[i]    += other.cutoff_index_sum[i];
            cutoff_index_1[i]      += other.cutoff_index_1[i];
            cutoff_index_2[i]      += other.cutoff_index_2[i];
            cutoff_index_3_4[i]    += other.cutoff_index_3_4[i];
            cutoff_index_5_plus[i] += other.cutoff_index_5_plus[i];
            pvs_researches[i]      += other.pvs_researches[i];
            main_tt_probes[i]      += other.main_tt_probes[i];
            main_tt_hits[i]        += other.main_tt_hits[i];
            main_tt_cutoffs[i]     += other.main_tt_cutoffs[i];
            q_tt_probes[i]         += other.q_tt_probes[i];
            q_tt_hits[i]           += other.q_tt_hits[i];
            q_tt_cutoffs[i]        += other.q_tt_cutoffs[i];

            staged_nodes[i]                     += other.staged_nodes[i];
            staged_noisy_generated_nodes[i]     += other.staged_noisy_generated_nodes[i];
            staged_quiet_generated_nodes[i]     += other.staged_quiet_generated_nodes[i];
            staged_skip_quiet_nodes[i]          += other.staged_skip_quiet_nodes[i];
            staged_cutoff_before_quiet_nodes[i] += other.staged_cutoff_before_quiet_nodes[i];
            staged_cutoff_quiet_nodes[i]        += other.staged_cutoff_quiet_nodes[i];
            staged_cutoff_bad_noisy_nodes[i]    += other.staged_cutoff_bad_noisy_nodes[i];
        }

        aspiration_fail_lows  += other.aspiration_fail_lows;
        aspiration_fail_highs += other.aspiration_fail_highs;
        aspiration_researches += other.aspiration_researches;
        return *this;
    }

    SearchStats operator+(const SearchStats& other) const {
        SearchStats result  = *this;
        result             += other;
        return result;
    }

private:
    static bool valid_ply(const int ply) { return ply >= 0 && ply < MAX_SEARCH_PLY; }
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
                             "\nAspiration: fail-low={} fail-high={} re-searches={}\n",
                             stats.aspiration_fail_lows,
                             stats.aspiration_fail_highs,
                             stats.aspiration_researches);

        const uint64_t staged_nodes               = sum(stats.staged_nodes);
        const uint64_t staged_noisy_generated     = sum(stats.staged_noisy_generated_nodes);
        const uint64_t staged_quiet_generated     = sum(stats.staged_quiet_generated_nodes);
        const uint64_t staged_skip_quiet          = sum(stats.staged_skip_quiet_nodes);
        const uint64_t staged_cutoff_before_quiet = sum(stats.staged_cutoff_before_quiet_nodes);
        const uint64_t staged_cutoff_quiet        = sum(stats.staged_cutoff_quiet_nodes);
        const uint64_t staged_cutoff_bad_noisy    = sum(stats.staged_cutoff_bad_noisy_nodes);

        out = std::format_to(out,
                             "StagedPicker: nodes={} noisy-generated={} ({:.1f}%) "
                             "quiet-generated={} ({:.1f}%) skip-quiet={} ({:.1f}%) "
                             "cutoff-before-quiet={} ({:.1f}%) "
                             "cutoff-quiet={} ({:.1f}%) cutoff-bad-noisy={} ({:.1f}%)\n",
                             staged_nodes,
                             staged_noisy_generated,
                             pct(staged_noisy_generated, staged_nodes),
                             staged_quiet_generated,
                             pct(staged_quiet_generated, staged_nodes),
                             staged_skip_quiet,
                             pct(staged_skip_quiet, staged_nodes),
                             staged_cutoff_before_quiet,
                             pct(staged_cutoff_before_quiet, staged_nodes),
                             staged_cutoff_quiet,
                             pct(staged_cutoff_quiet, staged_nodes),
                             staged_cutoff_bad_noisy,
                             pct(staged_cutoff_bad_noisy, staged_nodes));

        out =
            std::format_to(out,
                           "{:>5} | {:>18} | {:>23} | {:>27} | {:>6} | {:>15} | {:>13} | {:>13}\n",
                           "Depth",
                           "Nodes (QNode%)",
                           "Cutoffs (Early%/Late%)",
                           "CutIdx Avg/1/2/3-4/5+%",
                           "PVS Re",
                           "MainTT Hit/Cut%",
                           "QTT Hit/Cut%",
                           "EBF / Cumul");

        int maxDepth = MAX_SEARCH_PLY - 1;
        while (maxDepth > 0 && stats.nodes[maxDepth] == 0)
            --maxDepth;

        for (size_t d = 1; d <= static_cast<size_t>(maxDepth); ++d) {
            uint64_t nodes          = stats.nodes[d];
            uint64_t prev           = d > 1 ? stats.nodes[d - 1] : 0;
            uint64_t qnodes         = stats.qnodes[d];
            uint64_t cutoffs        = stats.cutoffs[d];
            uint64_t early          = stats.fail_high_early[d];
            uint64_t late           = stats.fail_high_late[d];
            uint64_t pvs_researches = stats.pvs_researches[d];

            double qnode_pct      = pct(qnodes, nodes);
            double early_pct      = pct(early, cutoffs);
            double later_pct      = pct(late, cutoffs);
            double cutoff_avg     = cutoffs > 0 ? double(stats.cutoff_index_sum[d]) / cutoffs : 0.0;
            double cutoff_1_pct   = pct(stats.cutoff_index_1[d], cutoffs);
            double cutoff_2_pct   = pct(stats.cutoff_index_2[d], cutoffs);
            double cutoff_3_4_pct = pct(stats.cutoff_index_3_4[d], cutoffs);
            double cutoff_5_pct   = pct(stats.cutoff_index_5_plus[d], cutoffs);
            double main_tt_hit_pct = pct(stats.main_tt_hits[d], stats.main_tt_probes[d]);
            double main_tt_cut_pct = pct(stats.main_tt_cutoffs[d], stats.main_tt_hits[d]);
            double q_tt_hit_pct    = pct(stats.q_tt_hits[d], stats.q_tt_probes[d]);
            double q_tt_cut_pct    = pct(stats.q_tt_cutoffs[d], stats.q_tt_hits[d]);
            double ebf             = prev > 0 ? static_cast<double>(nodes) / prev : 0.0;
            double cumulative      = std::pow(static_cast<double>(nodes), 1.0 / d);

            out = std::format_to(out, "{:>5} | ", d);
            out = std::format_to(out, "{:9} ({:5.1f}%) | ", nodes, qnode_pct);
            out = std::format_to(out, "{:8} ({:5.1f}/{:5.1f}%) | ", cutoffs, early_pct, later_pct);
            out = std::format_to(out,
                                 "{:4.1f} / {:5.1f}/{:5.1f}/{:5.1f}/{:5.1f}% | ",
                                 cutoff_avg,
                                 cutoff_1_pct,
                                 cutoff_2_pct,
                                 cutoff_3_4_pct,
                                 cutoff_5_pct);
            out = std::format_to(out, "{:6} | ", pvs_researches);
            out = std::format_to(out, "{:5.1f}/{:5.1f}% | ", main_tt_hit_pct, main_tt_cut_pct);
            out = std::format_to(out, "{:5.1f}/{:5.1f}% | ", q_tt_hit_pct, q_tt_cut_pct);
            out = std::format_to(out, "{:5.1f} / {:5.1f}\n", ebf, cumulative);
        }

        return out;
    }

private:
    static double pct(const uint64_t count, const uint64_t total) {
        return total > 0 ? 100.0 * count / total : 0.0;
    }

    static uint64_t sum(const SearchStats<true>::StatsArray& values) {
        uint64_t total = 0;
        for (uint64_t value : values)
            total += value;
        return total;
    }
};
