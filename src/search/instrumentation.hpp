#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "core/constants.hpp"

#ifndef LATRUNCULI_SEARCH_STATS
#define LATRUNCULI_SEARCH_STATS 0
#endif

namespace search {

constexpr bool stats_enabled = LATRUNCULI_SEARCH_STATS;

struct Counters {
    using PerPly = std::array<std::uint64_t, engine::max_search_ply>;
    PerPly        nodes{}, qnodes{};
    std::uint64_t beta_cutoffs{0}, first_move_cutoffs{0};
    PerPly        pvs_researches{};
    std::uint64_t aspiration_fail_lows{0}, aspiration_fail_highs{0};
    PerPly        main_tt_probes{}, main_tt_hits{}, main_tt_cutoffs{};
    PerPly        q_tt_probes{}, q_tt_hits{}, q_tt_cutoffs{};
    PerPly        null_move_tries{}, null_move_cutoffs{};
    PerPly        razor_tries{}, razor_cutoffs{}, futility_skips{};
    PerPly        lmr_tries{}, lmr_researches{};
    std::uint64_t quiet_cutoffs{0}, quiet_malus_eligible_nodes{0};
    std::uint64_t quiet_malus_failed_quiets{0}, quiet_malus_updates{0};
};

template <bool Enable = stats_enabled>
class Instrumentation;

template <>
class Instrumentation<false> {
public:
    void             reset() {}
    void             node(int) {}
    void             qnode(int) {}
    void             beta_cutoff(int, int) {}
    void             pvs_research(int) {}
    void             aspiration_fail_low() {}
    void             aspiration_fail_high() {}
    void             main_tt_probe(int) {}
    void             main_tt_hit(int) {}
    void             main_tt_cutoff(int) {}
    void             q_tt_probe(int) {}
    void             q_tt_hit(int) {}
    void             q_tt_cutoff(int) {}
    void             null_move_try(int) {}
    void             null_move_cutoff(int) {}
    void             razor_try(int) {}
    void             razor_cutoff(int) {}
    void             futility_skip(int) {}
    void             lmr_try(int) {}
    void             lmr_research(int) {}
    void             quiet_cutoff(int) {}
    void             quiet_malus_eligible_node(int) {}
    void             quiet_malus_failed_quiet(int) {}
    void             quiet_malus_update(int) {}
    std::string      str() const { return {}; }
    Instrumentation& operator+=(const Instrumentation&) { return *this; }
};

template <>
class Instrumentation<true> {
public:
    Instrumentation() = default;
    explicit Instrumentation(const Counters& values) : counters(values) {}

    void reset() { counters = {}; }
    void node(int ply) {
        if (valid(ply))
            ++counters.nodes[ply];
    }
    void qnode(int ply) {
        if (valid(ply)) {
            ++counters.nodes[ply];
            ++counters.qnodes[ply];
        }
    }
    void beta_cutoff(int ply, int move_index) {
        if (valid(ply) && move_index > 0) {
            ++counters.beta_cutoffs;
            if (move_index == 1)
                ++counters.first_move_cutoffs;
        }
    }
    void pvs_research(int ply) {
        if (valid(ply))
            ++counters.pvs_researches[ply];
    }
    void aspiration_fail_low() { ++counters.aspiration_fail_lows; }
    void aspiration_fail_high() { ++counters.aspiration_fail_highs; }
    void main_tt_probe(int ply) {
        if (valid(ply))
            ++counters.main_tt_probes[ply];
    }
    void main_tt_hit(int ply) {
        if (valid(ply))
            ++counters.main_tt_hits[ply];
    }
    void main_tt_cutoff(int ply) {
        if (valid(ply))
            ++counters.main_tt_cutoffs[ply];
    }
    void q_tt_probe(int ply) {
        if (valid(ply))
            ++counters.q_tt_probes[ply];
    }
    void q_tt_hit(int ply) {
        if (valid(ply))
            ++counters.q_tt_hits[ply];
    }
    void q_tt_cutoff(int ply) {
        if (valid(ply))
            ++counters.q_tt_cutoffs[ply];
    }
    void null_move_try(int ply) {
        if (valid(ply))
            ++counters.null_move_tries[ply];
    }
    void null_move_cutoff(int ply) {
        if (valid(ply))
            ++counters.null_move_cutoffs[ply];
    }
    void razor_try(int ply) {
        if (valid(ply))
            ++counters.razor_tries[ply];
    }
    void razor_cutoff(int ply) {
        if (valid(ply))
            ++counters.razor_cutoffs[ply];
    }
    void futility_skip(int ply) {
        if (valid(ply))
            ++counters.futility_skips[ply];
    }
    void lmr_try(int ply) {
        if (valid(ply))
            ++counters.lmr_tries[ply];
    }
    void lmr_research(int ply) {
        if (valid(ply))
            ++counters.lmr_researches[ply];
    }
    void quiet_cutoff(int depth) {
        if (valid(depth))
            ++counters.quiet_cutoffs;
    }
    void quiet_malus_eligible_node(int depth) {
        if (valid(depth))
            ++counters.quiet_malus_eligible_nodes;
    }
    void quiet_malus_failed_quiet(int depth) {
        if (valid(depth))
            ++counters.quiet_malus_failed_quiets;
    }
    void quiet_malus_update(int depth) {
        if (valid(depth))
            ++counters.quiet_malus_updates;
    }

    Instrumentation& operator+=(const Instrumentation& other);
    const Counters&  raw_counters() const { return counters; }
    std::string      str() const;

private:
    static bool valid(int index) { return index >= 0 && index < engine::max_search_ply; }
    Counters    counters;
};

} // namespace search
