#include "search/instrumentation.hpp"

#if LATRUNCULI_SEARCH_STATS

#include <cmath>
#include <format>
#include <iterator>

namespace search {

namespace {

std::uint64_t sum(const Counters::CounterArray& values) {
    std::uint64_t total = 0;
    for (const std::uint64_t value : values)
        total += value;
    return total;
}

double percentage(const std::uint64_t count, const std::uint64_t total) {
    return total > 0 ? 100.0 * count / total : 0.0;
}

bool has_quiet_history_stats(const Counters& stats, const int depth) {
    return stats.quiet_cutoffs[depth] != 0 || stats.quiet_malus_eligible_nodes[depth] != 0
        || stats.quiet_malus_failed_quiets[depth] != 0 || stats.quiet_malus_updates[depth] != 0;
}

int max_quiet_history_depth(const Counters& stats) {
    for (int depth = engine::max_search_ply - 1; depth > 0; --depth) {
        if (has_quiet_history_stats(stats, depth))
            return depth;
    }
    return 0;
}

} // namespace

void Instrumentation<true>::reset() {
    counters = {};
}

Instrumentation<true>& Instrumentation<true>::operator+=(const Instrumentation& other) {
    for (std::size_t i = 0; i < engine::max_search_ply; ++i) {
        counters.nodes[i] += other.counters.nodes[i];
        counters.qnodes[i] += other.counters.qnodes[i];
        counters.cutoff_index_sum[i] += other.counters.cutoff_index_sum[i];
        counters.cutoff_index_1[i] += other.counters.cutoff_index_1[i];
        counters.cutoff_index_2[i] += other.counters.cutoff_index_2[i];
        counters.cutoff_index_3_4[i] += other.counters.cutoff_index_3_4[i];
        counters.cutoff_index_5_plus[i] += other.counters.cutoff_index_5_plus[i];
        counters.pvs_researches[i] += other.counters.pvs_researches[i];
        counters.main_tt_probes[i] += other.counters.main_tt_probes[i];
        counters.main_tt_hits[i] += other.counters.main_tt_hits[i];
        counters.main_tt_cutoffs[i] += other.counters.main_tt_cutoffs[i];
        counters.q_tt_probes[i] += other.counters.q_tt_probes[i];
        counters.q_tt_hits[i] += other.counters.q_tt_hits[i];
        counters.q_tt_cutoffs[i] += other.counters.q_tt_cutoffs[i];
        counters.null_move_tries[i] += other.counters.null_move_tries[i];
        counters.null_move_cutoffs[i] += other.counters.null_move_cutoffs[i];
        counters.razor_tries[i] += other.counters.razor_tries[i];
        counters.razor_cutoffs[i] += other.counters.razor_cutoffs[i];
        counters.futility_skips[i] += other.counters.futility_skips[i];
        counters.lmr_tries[i] += other.counters.lmr_tries[i];
        counters.lmr_researches[i] += other.counters.lmr_researches[i];
        counters.quiet_cutoffs[i] += other.counters.quiet_cutoffs[i];
        counters.quiet_malus_eligible_nodes[i] += other.counters.quiet_malus_eligible_nodes[i];
        counters.quiet_malus_failed_quiets[i] += other.counters.quiet_malus_failed_quiets[i];
        counters.quiet_malus_updates[i] += other.counters.quiet_malus_updates[i];
    }

    counters.aspiration_fail_lows += other.counters.aspiration_fail_lows;
    counters.aspiration_fail_highs += other.counters.aspiration_fail_highs;
    return *this;
}

std::string Instrumentation<true>::str() const {
    std::string report;
    auto        out = std::back_inserter(report);

    const std::uint64_t re_searches =
        counters.aspiration_fail_lows + counters.aspiration_fail_highs;
    out = std::format_to(out,
                         "\nAspiration: fail-low={} fail-high={} re-searches={}\n",
                         counters.aspiration_fail_lows,
                         counters.aspiration_fail_highs,
                         re_searches);

    const std::uint64_t null_move_tries   = sum(counters.null_move_tries);
    const std::uint64_t null_move_cutoffs = sum(counters.null_move_cutoffs);

    out = std::format_to(out,
                         "NullMove: tries={} cutoffs={} cutoff-rate={:.1f}%\n",
                         null_move_tries,
                         null_move_cutoffs,
                         percentage(null_move_cutoffs, null_move_tries));

    const std::uint64_t razor_tries    = sum(counters.razor_tries);
    const std::uint64_t razor_cutoffs  = sum(counters.razor_cutoffs);
    const std::uint64_t futility_skips = sum(counters.futility_skips);

    out = std::format_to(out,
                         "RazorFutility: razor-tries={} razor-cutoffs={} "
                         "razor-cutoff-rate={:.1f}% futility-skips={}\n",
                         razor_tries,
                         razor_cutoffs,
                         percentage(razor_cutoffs, razor_tries),
                         futility_skips);

    const std::uint64_t lmr_tries      = sum(counters.lmr_tries);
    const std::uint64_t lmr_researches = sum(counters.lmr_researches);

    out = std::format_to(out,
                         "LMR: tries={} re-searches={} re-search-rate={:.1f}%\n",
                         lmr_tries,
                         lmr_researches,
                         percentage(lmr_researches, lmr_tries));

    const std::uint64_t quiet_cutoffs              = sum(counters.quiet_cutoffs);
    const std::uint64_t quiet_malus_eligible_nodes = sum(counters.quiet_malus_eligible_nodes);
    const std::uint64_t quiet_malus_failed_quiets  = sum(counters.quiet_malus_failed_quiets);
    const std::uint64_t quiet_malus_updates        = sum(counters.quiet_malus_updates);

    out = std::format_to(out,
                         "QuietHistory: quiet-cutoffs={} malus-eligible={} failed-quiets={} "
                         "malus-updates={}\n",
                         quiet_cutoffs,
                         quiet_malus_eligible_nodes,
                         quiet_malus_failed_quiets,
                         quiet_malus_updates);

    const int max_qhist_depth = max_quiet_history_depth(counters);
    if (max_qhist_depth > 0) {
        out = std::format_to(out,
                             "{:>5} | {:>13} | {:>13} | {:>13} | {:>13}\n",
                             "QH D",
                             "Cutoffs",
                             "Eligible",
                             "FailedQuiet",
                             "MalusUpdate");
        for (int depth = 1; depth <= max_qhist_depth; ++depth) {
            if (!has_quiet_history_stats(counters, depth))
                continue;

            out = std::format_to(out,
                                 "{:>5} | {:>13} | {:>13} | {:>13} | {:>13}\n",
                                 depth,
                                 counters.quiet_cutoffs[depth],
                                 counters.quiet_malus_eligible_nodes[depth],
                                 counters.quiet_malus_failed_quiets[depth],
                                 counters.quiet_malus_updates[depth]);
        }
    }

    out = std::format_to(out,
                         "{:>5} | {:>18} | {:>23} | {:>27} | {:>6} | {:>15} | {:>13} | "
                         "{:>13}\n",
                         "Ply",
                         "Nodes (QNode%)",
                         "Cutoffs (Early%/Late%)",
                         "CutIdx Avg/1/2/3-4/5+%",
                         "PVS Re",
                         "MainTT Hit/Cut%",
                         "QTT Hit/Cut%",
                         "EBF / Cumul");

    int max_ply = engine::max_search_ply - 1;
    while (max_ply > 0 && counters.nodes[max_ply] == 0)
        --max_ply;

    for (std::size_t ply = 1; ply <= static_cast<std::size_t>(max_ply); ++ply) {
        const std::uint64_t nodes   = counters.nodes[ply];
        const std::uint64_t prev    = ply > 1 ? counters.nodes[ply - 1] : 0;
        const std::uint64_t qnodes  = counters.qnodes[ply];
        const std::uint64_t cutoffs = counters.cutoff_index_1[ply] + counters.cutoff_index_2[ply]
                                    + counters.cutoff_index_3_4[ply]
                                    + counters.cutoff_index_5_plus[ply];
        const std::uint64_t early          = counters.cutoff_index_1[ply];
        const std::uint64_t late           = cutoffs - early;
        const std::uint64_t pvs_researches = counters.pvs_researches[ply];

        const double qnode_pct = percentage(qnodes, nodes);
        const double early_pct = percentage(early, cutoffs);
        const double later_pct = percentage(late, cutoffs);
        const double cutoff_avg =
            cutoffs > 0 ? double(counters.cutoff_index_sum[ply]) / cutoffs : 0.0;
        const double cutoff_1_pct   = percentage(counters.cutoff_index_1[ply], cutoffs);
        const double cutoff_2_pct   = percentage(counters.cutoff_index_2[ply], cutoffs);
        const double cutoff_3_4_pct = percentage(counters.cutoff_index_3_4[ply], cutoffs);
        const double cutoff_5_pct   = percentage(counters.cutoff_index_5_plus[ply], cutoffs);
        const double main_tt_hit_pct =
            percentage(counters.main_tt_hits[ply], counters.main_tt_probes[ply]);
        const double main_tt_cut_pct =
            percentage(counters.main_tt_cutoffs[ply], counters.main_tt_hits[ply]);
        const double q_tt_hit_pct = percentage(counters.q_tt_hits[ply], counters.q_tt_probes[ply]);
        const double q_tt_cut_pct = percentage(counters.q_tt_cutoffs[ply], counters.q_tt_hits[ply]);
        const double ebf          = prev > 0 ? static_cast<double>(nodes) / prev : 0.0;
        const double cumulative   = std::pow(static_cast<double>(nodes), 1.0 / ply);

        out = std::format_to(out, "{:>5} | ", ply);
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

    return report;
}

} // namespace search

#endif
