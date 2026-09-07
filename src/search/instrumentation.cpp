#include "search/instrumentation.hpp"

#if LATRUNCULI_SEARCH_STATS

#include <cassert>
#include <cmath>
#include <format>
#include <iterator>
#include <string_view>

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

std::string_view name(const CaptureOrderContext context) {
    switch (context) {
    case CaptureOrderContext::Main:    return "main";
    case CaptureOrderContext::Qsearch: return "qsearch";
    case CaptureOrderContext::Count:   break;
    }
    return "unknown";
}

std::string_view name(const CaptureOrderNode node) {
    switch (node) {
    case CaptureOrderNode::Pv:    return "pv";
    case CaptureOrderNode::NonPv: return "nonpv";
    case CaptureOrderNode::Count: break;
    }
    return "unknown";
}

std::string_view name(const CaptureOrderStage stage) {
    switch (stage) {
    case CaptureOrderStage::Tt:              return "tt";
    case CaptureOrderStage::QsearchTtSeeBad: return "q-tt-see-bad";
    case CaptureOrderStage::Evasion:         return "evasion";
    case CaptureOrderStage::Promotion:       return "promotion";
    case CaptureOrderStage::ExactSeeGood:    return "exact-see-good";
    case CaptureOrderStage::ExactSeeBad:     return "exact-see-bad";
    case CaptureOrderStage::Count:           break;
    }
    return "unknown";
}

std::string_view name(const CaptureOrderMove move) {
    switch (move) {
    case CaptureOrderMove::OrdinaryCapture: return "ordinary-capture";
    case CaptureOrderMove::EnPassant:       return "en-passant";
    case CaptureOrderMove::Promotion:       return "promotion";
    case CaptureOrderMove::Quiet:           return "quiet";
    case CaptureOrderMove::Count:           break;
    }
    return "unknown";
}

std::string_view name(const CaptureOrderBucket bucket) {
    switch (bucket) {
    case CaptureOrderBucket::One:       return "1";
    case CaptureOrderBucket::Two:       return "2";
    case CaptureOrderBucket::ThreeFour: return "3-4";
    case CaptureOrderBucket::FiveEight: return "5-8";
    case CaptureOrderBucket::NinePlus:  return "9+";
    case CaptureOrderBucket::Count:     break;
    }
    return "unknown";
}

std::string_view name(const LmrNode node) {
    switch (node) {
    case LmrNode::Pv:    return "pv";
    case LmrNode::NonPv: return "nonpv";
    case LmrNode::Count: break;
    }
    return "unknown";
}

std::string_view name(const LmrMoveClass move_class) {
    switch (move_class) {
    case LmrMoveClass::Quiet: return "quiet";
    case LmrMoveClass::Noisy: return "noisy";
    case LmrMoveClass::Count: break;
    }
    return "unknown";
}

std::string_view name(const LmrReductionBucket reduction) {
    switch (reduction) {
    case LmrReductionBucket::One:       return "1";
    case LmrReductionBucket::Two:       return "2";
    case LmrReductionBucket::ThreePlus: return "3+";
    case LmrReductionBucket::Count:     break;
    }
    return "unknown";
}

std::string_view name(const LmrHistoryBucket history) {
    switch (history) {
    case LmrHistoryBucket::LeNegative1024: return "le-neg-1024";
    case LmrHistoryBucket::Negative:       return "negative";
    case LmrHistoryBucket::Zero:           return "zero";
    case LmrHistoryBucket::Positive:       return "positive";
    case LmrHistoryBucket::Ge1024:         return "ge-1024";
    case LmrHistoryBucket::NotApplicable:  return "na";
    case LmrHistoryBucket::Count:          break;
    }
    return "unknown";
}

} // namespace

void LmrVerifier::reset(const std::optional<std::uint64_t> configured_target) {
    assert(!configured_target || *configured_target == 0
           || (*configured_target >= occurrence_stride && *configured_target <= max_occurrence
               && *configured_target % occurrence_stride == 0));

    target       = configured_target;
    occurrences  = 0;
    samples      = {};
    sample_count = 0;
    suppressed   = false;
    reached      = false;
    contaminated = false;
    full_value   = 0;
    nodes_after  = 0;
}

bool LmrVerifier::observe_fail_low(const LmrVerifierObservation& observation) {
    if (!target || suppressed || reached || observation.node == LmrNode::Count
        || observation.history_score < 1024 || observation.reduced_value > observation.alpha)
        return false;

    ++occurrences;
    if (occurrences > max_occurrence || occurrences % occurrence_stride != 0)
        return false;

    assert(sample_count < samples.size());
    Sample& sample                               = samples[sample_count++];
    static_cast<LmrVerifierObservation&>(sample) = observation;
    sample.occurrence                            = occurrences;

    const bool activate = *target != 0 && occurrences == *target;
    if (activate) {
        suppressed   = true;
        contaminated = true;
    }
    return activate;
}

void LmrVerifier::complete(const EvalValue verified_value, const NodeCount verified_nodes_after) {
    assert(target && *target != 0 && suppressed && sample_count > 0
           && samples[sample_count - 1].occurrence == *target);
    assert(verified_nodes_after > samples[sample_count - 1].nodes);
    full_value  = verified_value;
    nodes_after = verified_nodes_after;
    reached     = true;
}

std::string LmrVerifier::str() const {
    if (!target)
        return {};

    std::string report;
    auto        out    = std::back_inserter(report);
    const bool  active = *target != 0;
    out                = std::format_to(
        out, "LmrVerifier: schema=1 mode={} target={}\n", active ? "active" : "control", *target);

    for (std::size_t index = 0; index < sample_count; ++index) {
        const Sample& sample = samples[index];
        out                  = std::format_to(
            out,
            "LmrVerifierSample: occurrence={} parent-key={:016x} move={} node={} ply={} "
                             "depth={} reduction={} history={} alpha={} beta={} reduced-value={} nodes={}\n",
            sample.occurrence,
            sample.parent_key,
            sample.move.str(),
            name(sample.node),
            sample.ply,
            sample.depth,
            sample.reduction,
            sample.history_score,
            sample.alpha,
            sample.beta,
            sample.reduced_value,
            sample.nodes);
    }

    if (reached) {
        const Sample& sample = samples[sample_count - 1];
        out                  = std::format_to(out,
                             "LmrVerifierResult: occurrence={} full-value={} nodes-before={} "
                                              "nodes-after={} outcome={}\n",
                             sample.occurrence,
                             full_value,
                             sample.nodes,
                             nodes_after,
                             full_value <= sample.alpha ? "confirmed" : "false-fail-low");
    }

    out = std::format_to(
        out,
        "LmrVerifierTotal: schema=1 samples={} target={} reached={} contaminated={}\n",
        sample_count,
        *target,
        reached ? 1 : 0,
        contaminated ? 1 : 0);
    return report;
}

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

    for (std::size_t i = 0; i < capture_order_cell_count; ++i) {
        counters.capture_order[i].fail_lows += other.counters.capture_order[i].fail_lows;
        counters.capture_order[i].alpha_raises += other.counters.capture_order[i].alpha_raises;
        counters.capture_order[i].beta_cutoffs += other.counters.capture_order[i].beta_cutoffs;
        counters.capture_order[i].success_ordinal_sum +=
            other.counters.capture_order[i].success_ordinal_sum;
    }

    for (std::size_t i = 0; i < lmr_history_cell_count; ++i) {
        counters.lmr_history[i].attempts += other.counters.lmr_history[i].attempts;
        counters.lmr_history[i].reduced_interrupted +=
            other.counters.lmr_history[i].reduced_interrupted;
        counters.lmr_history[i].reduced_fail_lows +=
            other.counters.lmr_history[i].reduced_fail_lows;
        counters.lmr_history[i].reduced_alpha_raises +=
            other.counters.lmr_history[i].reduced_alpha_raises;
        counters.lmr_history[i].research_interrupted +=
            other.counters.lmr_history[i].research_interrupted;
        counters.lmr_history[i].research_refuted += other.counters.lmr_history[i].research_refuted;
        counters.lmr_history[i].research_alpha_raises +=
            other.counters.lmr_history[i].research_alpha_raises;
        counters.lmr_history[i].research_cutoffs += other.counters.lmr_history[i].research_cutoffs;
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

    out = std::format_to(out, "CaptureOrdering: schema=1\n");

    std::size_t      emitted_cells = 0;
    CaptureOrderCell totals;
    for (std::size_t context_index = 0; context_index < capture_order_context_count;
         ++context_index) {
        const auto context = static_cast<CaptureOrderContext>(context_index);
        for (std::size_t node_index = 0; node_index < capture_order_node_count; ++node_index) {
            const auto node = static_cast<CaptureOrderNode>(node_index);
            for (std::size_t stage_index = 0; stage_index < capture_order_stage_count;
                 ++stage_index) {
                const auto stage = static_cast<CaptureOrderStage>(stage_index);
                for (std::size_t move_index = 0; move_index < capture_order_move_count;
                     ++move_index) {
                    const auto move = static_cast<CaptureOrderMove>(move_index);
                    for (std::size_t bucket_index = 0; bucket_index < capture_order_bucket_count;
                         ++bucket_index) {
                        const auto bucket = static_cast<CaptureOrderBucket>(bucket_index);
                        const CaptureOrderCell& cell = counters.capture_order[capture_order_index(
                            context, node, stage, move, bucket)];
                        const std::uint64_t     attempts =
                            cell.fail_lows + cell.alpha_raises + cell.beta_cutoffs;
                        if (attempts == 0)
                            continue;

                        ++emitted_cells;
                        totals.fail_lows += cell.fail_lows;
                        totals.alpha_raises += cell.alpha_raises;
                        totals.beta_cutoffs += cell.beta_cutoffs;
                        totals.success_ordinal_sum += cell.success_ordinal_sum;
                        out = std::format_to(
                            out,
                            "CaptureOrderingCell: context={} node={} stage={} move={} ordinal={} "
                            "attempts={} fail-low={} alpha-raise={} beta-cutoff={} "
                            "success-ordinal-sum={}\n",
                            name(context),
                            name(node),
                            name(stage),
                            name(move),
                            name(bucket),
                            attempts,
                            cell.fail_lows,
                            cell.alpha_raises,
                            cell.beta_cutoffs,
                            cell.success_ordinal_sum);
                    }
                }
            }
        }
    }

    const std::uint64_t total_attempts =
        totals.fail_lows + totals.alpha_raises + totals.beta_cutoffs;
    out = std::format_to(out,
                         "CaptureOrderingTotal: schema=1 cells={} attempts={} fail-low={} "
                         "alpha-raise={} beta-cutoff={} success-ordinal-sum={}\n",
                         emitted_cells,
                         total_attempts,
                         totals.fail_lows,
                         totals.alpha_raises,
                         totals.beta_cutoffs,
                         totals.success_ordinal_sum);

    out                      = std::format_to(out, "LmrHistory: schema=1\n");
    std::size_t    lmr_cells = 0;
    LmrHistoryCell lmr_totals;
    for (std::size_t node_index = 0; node_index < lmr_node_count; ++node_index) {
        const auto node = static_cast<LmrNode>(node_index);
        for (std::size_t move_index = 0; move_index < lmr_move_class_count; ++move_index) {
            const auto move_class = static_cast<LmrMoveClass>(move_index);
            for (int depth = 3; depth <= engine::max_search_depth; ++depth) {
                for (std::size_t reduction_index = 0; reduction_index < lmr_reduction_bucket_count;
                     ++reduction_index) {
                    const auto reduction = static_cast<LmrReductionBucket>(reduction_index);
                    for (std::size_t history_index = 0; history_index < lmr_history_bucket_count;
                         ++history_index) {
                        const auto history = static_cast<LmrHistoryBucket>(history_index);
                        if ((move_class == LmrMoveClass::Quiet)
                            == (history == LmrHistoryBucket::NotApplicable))
                            continue;

                        const LmrHistoryCell& cell = counters.lmr_history[lmr_history_index(
                            node, move_class, depth, reduction, history)];
                        if (cell.attempts == 0)
                            continue;

                        ++lmr_cells;
                        lmr_totals.attempts += cell.attempts;
                        lmr_totals.reduced_interrupted += cell.reduced_interrupted;
                        lmr_totals.reduced_fail_lows += cell.reduced_fail_lows;
                        lmr_totals.reduced_alpha_raises += cell.reduced_alpha_raises;
                        lmr_totals.research_interrupted += cell.research_interrupted;
                        lmr_totals.research_refuted += cell.research_refuted;
                        lmr_totals.research_alpha_raises += cell.research_alpha_raises;
                        lmr_totals.research_cutoffs += cell.research_cutoffs;
                        out = std::format_to(
                            out,
                            "LmrHistoryCell: node={} move={} depth={} reduction={} history={} "
                            "attempts={} reduced-interrupted={} reduced-fail-low={} "
                            "reduced-alpha-raise={} research-interrupted={} "
                            "research-refuted={} research-alpha-raise={} research-cutoff={}\n",
                            name(node),
                            name(move_class),
                            depth,
                            name(reduction),
                            name(history),
                            cell.attempts,
                            cell.reduced_interrupted,
                            cell.reduced_fail_lows,
                            cell.reduced_alpha_raises,
                            cell.research_interrupted,
                            cell.research_refuted,
                            cell.research_alpha_raises,
                            cell.research_cutoffs);
                    }
                }
            }
        }
    }

    out = std::format_to(out,
                         "LmrHistoryTotal: schema=1 cells={} attempts={} reduced-interrupted={} "
                         "reduced-fail-low={} reduced-alpha-raise={} research-interrupted={} "
                         "research-refuted={} research-alpha-raise={} research-cutoff={}\n",
                         lmr_cells,
                         lmr_totals.attempts,
                         lmr_totals.reduced_interrupted,
                         lmr_totals.reduced_fail_lows,
                         lmr_totals.reduced_alpha_raises,
                         lmr_totals.research_interrupted,
                         lmr_totals.research_refuted,
                         lmr_totals.research_alpha_raises,
                         lmr_totals.research_cutoffs);

    return report;
}

} // namespace search

#endif
