#include "search/instrumentation.hpp"

#if LATRUNCULI_SEARCH_STATS

#include <format>
#include <numeric>

namespace search {

namespace {
std::uint64_t sum(const Counters::PerPly& values) {
    return std::accumulate(values.begin(), values.end(), std::uint64_t{0});
}
} // namespace

Instrumentation<true>& Instrumentation<true>::operator+=(const Instrumentation& other) {
    for (std::size_t ply = 0; ply < engine::max_search_ply; ++ply) {
#define ADD_ARRAY(name) counters.name[ply] += other.counters.name[ply]
        ADD_ARRAY(nodes);
        ADD_ARRAY(qnodes);
        ADD_ARRAY(pvs_researches);
        ADD_ARRAY(main_tt_probes);
        ADD_ARRAY(main_tt_hits);
        ADD_ARRAY(main_tt_cutoffs);
        ADD_ARRAY(q_tt_probes);
        ADD_ARRAY(q_tt_hits);
        ADD_ARRAY(q_tt_cutoffs);
        ADD_ARRAY(null_move_tries);
        ADD_ARRAY(null_move_cutoffs);
        ADD_ARRAY(razor_tries);
        ADD_ARRAY(razor_cutoffs);
        ADD_ARRAY(futility_skips);
        ADD_ARRAY(lmr_tries);
        ADD_ARRAY(lmr_researches);
#undef ADD_ARRAY
    }
#define ADD(name) counters.name += other.counters.name
    ADD(beta_cutoffs);
    ADD(first_move_cutoffs);
    ADD(aspiration_fail_lows);
    ADD(aspiration_fail_highs);
    ADD(quiet_cutoffs);
    ADD(quiet_malus_eligible_nodes);
    ADD(quiet_malus_failed_quiets);
    ADD(quiet_malus_updates);
#undef ADD
    return *this;
}

std::string Instrumentation<true>::str() const {
    const Counters& c = counters;
    return std::format(
        "\nNodes: total={} qnodes={} beta-cutoffs={} first-move-cutoffs={} pvs-researches={}\n"
        "Aspiration: fail-low={} fail-high={}\n"
        "TT: main probes={} hits={} cutoffs={}; q probes={} hits={} cutoffs={}\n"
        "Pruning: null tries={} cutoffs={}; razor tries={} cutoffs={}; futility-skips={}\n"
        "LMR: tries={} re-searches={}\n"
        "QuietHistory: quiet-cutoffs={} malus-eligible={} failed-quiets={} malus-updates={}\n",
        sum(c.nodes),
        sum(c.qnodes),
        c.beta_cutoffs,
        c.first_move_cutoffs,
        sum(c.pvs_researches),
        c.aspiration_fail_lows,
        c.aspiration_fail_highs,
        sum(c.main_tt_probes),
        sum(c.main_tt_hits),
        sum(c.main_tt_cutoffs),
        sum(c.q_tt_probes),
        sum(c.q_tt_hits),
        sum(c.q_tt_cutoffs),
        sum(c.null_move_tries),
        sum(c.null_move_cutoffs),
        sum(c.razor_tries),
        sum(c.razor_cutoffs),
        sum(c.futility_skips),
        sum(c.lmr_tries),
        sum(c.lmr_researches),
        c.quiet_cutoffs,
        c.quiet_malus_eligible_nodes,
        c.quiet_malus_failed_quiets,
        c.quiet_malus_updates);
}

} // namespace search

#endif
