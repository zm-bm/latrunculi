#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include "core/defs.hpp"

#ifndef LATRUNCULI_SEARCH_STATS
#define LATRUNCULI_SEARCH_STATS 0
#endif

constexpr bool SEARCH_STATS_ENABLED = LATRUNCULI_SEARCH_STATS;

template <bool Enable = SEARCH_STATS_ENABLED>
struct SearchCounters;

template <bool Enable = SEARCH_STATS_ENABLED>
class SearchInstrumentation;

template <>
struct SearchCounters<false> {
    void            reset() {}
    SearchCounters& operator+=(const SearchCounters&) { return *this; }
    SearchCounters  operator+(const SearchCounters&) const { return *this; }
};

template <>
struct SearchCounters<true> {
    using StatsArray = std::array<uint64_t, engine::max_search_ply>;

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
    StatsArray null_move_tries{0};
    StatsArray null_move_cutoffs{0};
    StatsArray razor_tries{0};
    StatsArray razor_cutoffs{0};
    StatsArray futility_skips{0};
    StatsArray lmr_tries{0};
    StatsArray lmr_researches{0};
    StatsArray quiet_cutoffs{0};
    StatsArray quiet_malus_eligible_nodes{0};
    StatsArray quiet_malus_failed_quiets{0};
    StatsArray quiet_malus_updates{0};

    uint64_t aspiration_fail_lows{0};
    uint64_t aspiration_fail_highs{0};
    uint64_t aspiration_researches{0};

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
        null_move_tries.fill(0);
        null_move_cutoffs.fill(0);
        razor_tries.fill(0);
        razor_cutoffs.fill(0);
        futility_skips.fill(0);
        lmr_tries.fill(0);
        lmr_researches.fill(0);
        quiet_cutoffs.fill(0);
        quiet_malus_eligible_nodes.fill(0);
        quiet_malus_failed_quiets.fill(0);
        quiet_malus_updates.fill(0);
        aspiration_fail_lows  = 0;
        aspiration_fail_highs = 0;
        aspiration_researches = 0;
    }

    SearchCounters& operator+=(const SearchCounters& other) {
        for (size_t i = 0; i < engine::max_search_ply; ++i) {
            nodes[i]                      += other.nodes[i];
            qnodes[i]                     += other.qnodes[i];
            cutoffs[i]                    += other.cutoffs[i];
            fail_high_early[i]            += other.fail_high_early[i];
            fail_high_late[i]             += other.fail_high_late[i];
            cutoff_index_sum[i]           += other.cutoff_index_sum[i];
            cutoff_index_1[i]             += other.cutoff_index_1[i];
            cutoff_index_2[i]             += other.cutoff_index_2[i];
            cutoff_index_3_4[i]           += other.cutoff_index_3_4[i];
            cutoff_index_5_plus[i]        += other.cutoff_index_5_plus[i];
            pvs_researches[i]             += other.pvs_researches[i];
            main_tt_probes[i]             += other.main_tt_probes[i];
            main_tt_hits[i]               += other.main_tt_hits[i];
            main_tt_cutoffs[i]            += other.main_tt_cutoffs[i];
            q_tt_probes[i]                += other.q_tt_probes[i];
            q_tt_hits[i]                  += other.q_tt_hits[i];
            q_tt_cutoffs[i]               += other.q_tt_cutoffs[i];
            null_move_tries[i]            += other.null_move_tries[i];
            null_move_cutoffs[i]          += other.null_move_cutoffs[i];
            razor_tries[i]                += other.razor_tries[i];
            razor_cutoffs[i]              += other.razor_cutoffs[i];
            futility_skips[i]             += other.futility_skips[i];
            lmr_tries[i]                  += other.lmr_tries[i];
            lmr_researches[i]             += other.lmr_researches[i];
            quiet_cutoffs[i]              += other.quiet_cutoffs[i];
            quiet_malus_eligible_nodes[i] += other.quiet_malus_eligible_nodes[i];
            quiet_malus_failed_quiets[i]  += other.quiet_malus_failed_quiets[i];
            quiet_malus_updates[i]        += other.quiet_malus_updates[i];
        }

        aspiration_fail_lows  += other.aspiration_fail_lows;
        aspiration_fail_highs += other.aspiration_fail_highs;
        aspiration_researches += other.aspiration_researches;
        return *this;
    }

    SearchCounters operator+(const SearchCounters& other) const {
        SearchCounters result  = *this;
        result                += other;
        return result;
    }

    static bool valid_ply(const int ply) { return ply >= 0 && ply < engine::max_search_ply; }
};

template <>
class SearchInstrumentation<false> {
public:
    void        reset() {}
    void        node(int) {}
    void        qnode(int) {}
    void        beta_cutoff(int, int) {}
    void        pvs_research(int) {}
    void        aspiration_fail_low() {}
    void        aspiration_fail_high() {}
    void        aspiration_research() {}
    void        main_tt_probe(int) {}
    void        main_tt_hit(int) {}
    void        main_tt_cutoff(int) {}
    void        q_tt_probe(int) {}
    void        q_tt_hit(int) {}
    void        q_tt_cutoff(int) {}
    void        null_move_try(int) {}
    void        null_move_cutoff(int) {}
    void        razor_try(int) {}
    void        razor_cutoff(int) {}
    void        futility_skip(int) {}
    void        lmr_try(int) {}
    void        lmr_research(int) {}
    void        quiet_cutoff(int) {}
    void        quiet_malus_eligible_node(int) {}
    void        quiet_malus_failed_quiet(int) {}
    void        quiet_malus_update(int) {}
    std::string str() const;

    SearchInstrumentation& operator+=(const SearchInstrumentation&) { return *this; }
    SearchInstrumentation  operator+(const SearchInstrumentation&) const { return *this; }

private:
    SearchCounters<false> counters;
};

template <>
class SearchInstrumentation<true> {
public:
    SearchInstrumentation() = default;
    explicit SearchInstrumentation(SearchCounters<true> counters) : counters(counters) {}

    void reset() { counters.reset(); }

    void node(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.nodes[ply]++;
    }

    void qnode(const int ply) {
        if (SearchCounters<true>::valid_ply(ply)) {
            counters.nodes[ply]++;
            counters.qnodes[ply]++;
        }
    }

    void beta_cutoff(const int ply, const int move_index) {
        if (!SearchCounters<true>::valid_ply(ply) || move_index <= 0)
            return;

        counters.cutoffs[ply]++;
        counters.cutoff_index_sum[ply] += static_cast<uint64_t>(move_index);

        if (move_index == 1) {
            counters.fail_high_early[ply]++;
            counters.cutoff_index_1[ply]++;
        } else {
            counters.fail_high_late[ply]++;
            if (move_index == 2)
                counters.cutoff_index_2[ply]++;
            else if (move_index <= 4)
                counters.cutoff_index_3_4[ply]++;
            else
                counters.cutoff_index_5_plus[ply]++;
        }
    }

    void pvs_research(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.pvs_researches[ply]++;
    }

    void aspiration_fail_low() { counters.aspiration_fail_lows++; }
    void aspiration_fail_high() { counters.aspiration_fail_highs++; }
    void aspiration_research() { counters.aspiration_researches++; }

    void main_tt_probe(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.main_tt_probes[ply]++;
    }

    void main_tt_hit(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.main_tt_hits[ply]++;
    }

    void main_tt_cutoff(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.main_tt_cutoffs[ply]++;
    }

    void q_tt_probe(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.q_tt_probes[ply]++;
    }

    void q_tt_hit(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.q_tt_hits[ply]++;
    }

    void q_tt_cutoff(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.q_tt_cutoffs[ply]++;
    }

    void null_move_try(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.null_move_tries[ply]++;
    }

    void null_move_cutoff(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.null_move_cutoffs[ply]++;
    }

    void razor_try(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.razor_tries[ply]++;
    }

    void razor_cutoff(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.razor_cutoffs[ply]++;
    }

    void futility_skip(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.futility_skips[ply]++;
    }

    void lmr_try(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.lmr_tries[ply]++;
    }

    void lmr_research(const int ply) {
        if (SearchCounters<true>::valid_ply(ply))
            counters.lmr_researches[ply]++;
    }

    void quiet_cutoff(const int depth) {
        if (SearchCounters<true>::valid_ply(depth))
            counters.quiet_cutoffs[depth]++;
    }

    void quiet_malus_eligible_node(const int depth) {
        if (SearchCounters<true>::valid_ply(depth))
            counters.quiet_malus_eligible_nodes[depth]++;
    }

    void quiet_malus_failed_quiet(const int depth) {
        if (SearchCounters<true>::valid_ply(depth))
            counters.quiet_malus_failed_quiets[depth]++;
    }

    void quiet_malus_update(const int depth) {
        if (SearchCounters<true>::valid_ply(depth))
            counters.quiet_malus_updates[depth]++;
    }

    SearchInstrumentation& operator+=(const SearchInstrumentation& other) {
        counters += other.counters;
        return *this;
    }

    SearchInstrumentation operator+(const SearchInstrumentation& other) const {
        SearchInstrumentation result  = *this;
        result                       += other;
        return result;
    }

    const SearchCounters<true>& raw_counters() const { return counters; }
    std::string                 str() const;

private:
    SearchCounters<true> counters;
};

template <>
struct std::formatter<SearchInstrumentation<false>> : std::formatter<std::string_view> {
    auto format(const SearchInstrumentation<false>& instrumentation,
                std::format_context&                ctx) const {
        return ctx.out();
    }
};

template <>
struct std::formatter<SearchInstrumentation<true>> : std::formatter<std::string_view> {
    auto format(const SearchInstrumentation<true>& instrumentation,
                std::format_context&               ctx) const {
        const auto& stats = instrumentation.raw_counters();
        auto        out   = ctx.out();

        out = std::format_to(out,
                             "\nAspiration: fail-low={} fail-high={} re-searches={}\n",
                             stats.aspiration_fail_lows,
                             stats.aspiration_fail_highs,
                             stats.aspiration_researches);

        const uint64_t null_move_tries   = sum(stats.null_move_tries);
        const uint64_t null_move_cutoffs = sum(stats.null_move_cutoffs);
        out                              = std::format_to(out,
                             "NullMove: tries={} cutoffs={} cutoff-rate={:.1f}%\n",
                             null_move_tries,
                             null_move_cutoffs,
                             pct(null_move_cutoffs, null_move_tries));

        const uint64_t razor_tries    = sum(stats.razor_tries);
        const uint64_t razor_cutoffs  = sum(stats.razor_cutoffs);
        const uint64_t futility_skips = sum(stats.futility_skips);
        out                           = std::format_to(out,
                             "RazorFutility: razor-tries={} razor-cutoffs={} "
                                                       "razor-cutoff-rate={:.1f}% futility-skips={}\n",
                             razor_tries,
                             razor_cutoffs,
                             pct(razor_cutoffs, razor_tries),
                             futility_skips);

        const uint64_t lmr_tries      = sum(stats.lmr_tries);
        const uint64_t lmr_researches = sum(stats.lmr_researches);
        out                           = std::format_to(out,
                             "LMR: tries={} re-searches={} re-search-rate={:.1f}%\n",
                             lmr_tries,
                             lmr_researches,
                             pct(lmr_researches, lmr_tries));

        const uint64_t quiet_cutoffs              = sum(stats.quiet_cutoffs);
        const uint64_t quiet_malus_eligible_nodes = sum(stats.quiet_malus_eligible_nodes);
        const uint64_t quiet_malus_failed_quiets  = sum(stats.quiet_malus_failed_quiets);
        const uint64_t quiet_malus_updates        = sum(stats.quiet_malus_updates);
        out                                       = std::format_to(out,
                             "QuietHistory: quiet-cutoffs={} malus-eligible={} failed-quiets={} "
                                                                   "malus-updates={}\n",
                             quiet_cutoffs,
                             quiet_malus_eligible_nodes,
                             quiet_malus_failed_quiets,
                             quiet_malus_updates);

        const int max_qhist_depth = max_quiet_history_depth(stats);
        if (max_qhist_depth > 0) {
            out = std::format_to(out,
                                 "{:>5} | {:>13} | {:>13} | {:>13} | {:>13}\n",
                                 "QH D",
                                 "Cutoffs",
                                 "Eligible",
                                 "FailedQuiet",
                                 "MalusUpdate");
            for (int d = 1; d <= max_qhist_depth; ++d) {
                if (!has_quiet_history_stats(stats, d))
                    continue;

                out = std::format_to(out,
                                     "{:>5} | {:>13} | {:>13} | {:>13} | {:>13}\n",
                                     d,
                                     stats.quiet_cutoffs[d],
                                     stats.quiet_malus_eligible_nodes[d],
                                     stats.quiet_malus_failed_quiets[d],
                                     stats.quiet_malus_updates[d]);
            }
        }

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

        int maxDepth = engine::max_search_ply - 1;
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
    static uint64_t sum(const SearchCounters<true>::StatsArray& values) {
        uint64_t total = 0;
        for (const uint64_t value : values)
            total += value;
        return total;
    }

    static double pct(const uint64_t count, const uint64_t total) {
        return total > 0 ? 100.0 * count / total : 0.0;
    }

    static bool has_quiet_history_stats(const SearchCounters<true>& stats, int depth) {
        return stats.quiet_cutoffs[depth] != 0 || stats.quiet_malus_eligible_nodes[depth] != 0 ||
               stats.quiet_malus_failed_quiets[depth] != 0 || stats.quiet_malus_updates[depth] != 0;
    }

    static int max_quiet_history_depth(const SearchCounters<true>& stats) {
        for (int depth = engine::max_search_ply - 1; depth > 0; --depth) {
            if (has_quiet_history_stats(stats, depth))
                return depth;
        }
        return 0;
    }
};

inline std::string SearchInstrumentation<false>::str() const {
    return std::format("{}", *this);
}

inline std::string SearchInstrumentation<true>::str() const {
    return std::format("{}", *this);
}
