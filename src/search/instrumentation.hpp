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
    using CounterArray = std::array<std::uint64_t, engine::max_search_ply>;

    CounterArray nodes{0};
    CounterArray qnodes{0};

    CounterArray cutoff_index_sum{0};
    CounterArray cutoff_index_1{0};
    CounterArray cutoff_index_2{0};
    CounterArray cutoff_index_3_4{0};
    CounterArray cutoff_index_5_plus{0};

    CounterArray pvs_researches{0};

    CounterArray main_tt_probes{0};
    CounterArray main_tt_hits{0};
    CounterArray main_tt_cutoffs{0};
    CounterArray q_tt_probes{0};
    CounterArray q_tt_hits{0};
    CounterArray q_tt_cutoffs{0};
    CounterArray null_move_tries{0};
    CounterArray null_move_cutoffs{0};
    CounterArray razor_tries{0};
    CounterArray razor_cutoffs{0};
    CounterArray futility_skips{0};
    CounterArray lmr_tries{0};
    CounterArray lmr_researches{0};
    CounterArray quiet_cutoffs{0};
    CounterArray quiet_malus_eligible_nodes{0};
    CounterArray quiet_malus_failed_quiets{0};
    CounterArray quiet_malus_updates{0};

    std::uint64_t aspiration_fail_lows{0};
    std::uint64_t aspiration_fail_highs{0};
};

template <bool Enable = stats_enabled>
class Instrumentation;

template <>
class Instrumentation<false> {
public:
    void        reset() {}
    void        node(int) {}
    void        qnode(int) {}
    void        beta_cutoff(int, int) {}
    void        pvs_research(int) {}
    void        aspiration_fail_low() {}
    void        aspiration_fail_high() {}
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
    std::string str() const { return {}; }

    Instrumentation& operator+=(const Instrumentation&) { return *this; }
};

template <>
class Instrumentation<true> {
public:
    Instrumentation() = default;
    explicit Instrumentation(const Counters& values) : counters(values) {}

    void reset();

    void node(const int ply) {
        if (valid_index(ply))
            counters.nodes[ply]++;
    }

    void qnode(const int ply) {
        if (valid_index(ply)) {
            counters.nodes[ply]++;
            counters.qnodes[ply]++;
        }
    }

    void beta_cutoff(const int ply, const int move_index) {
        if (!valid_index(ply) || move_index <= 0)
            return;

        counters.cutoff_index_sum[ply] += static_cast<std::uint64_t>(move_index);

        if (move_index == 1)
            counters.cutoff_index_1[ply]++;
        else if (move_index == 2)
            counters.cutoff_index_2[ply]++;
        else if (move_index <= 4)
            counters.cutoff_index_3_4[ply]++;
        else
            counters.cutoff_index_5_plus[ply]++;
    }

    void pvs_research(const int ply) {
        if (valid_index(ply))
            counters.pvs_researches[ply]++;
    }

    void aspiration_fail_low() { counters.aspiration_fail_lows++; }
    void aspiration_fail_high() { counters.aspiration_fail_highs++; }

    void main_tt_probe(const int ply) {
        if (valid_index(ply))
            counters.main_tt_probes[ply]++;
    }

    void main_tt_hit(const int ply) {
        if (valid_index(ply))
            counters.main_tt_hits[ply]++;
    }

    void main_tt_cutoff(const int ply) {
        if (valid_index(ply))
            counters.main_tt_cutoffs[ply]++;
    }

    void q_tt_probe(const int ply) {
        if (valid_index(ply))
            counters.q_tt_probes[ply]++;
    }

    void q_tt_hit(const int ply) {
        if (valid_index(ply))
            counters.q_tt_hits[ply]++;
    }

    void q_tt_cutoff(const int ply) {
        if (valid_index(ply))
            counters.q_tt_cutoffs[ply]++;
    }

    void null_move_try(const int ply) {
        if (valid_index(ply))
            counters.null_move_tries[ply]++;
    }

    void null_move_cutoff(const int ply) {
        if (valid_index(ply))
            counters.null_move_cutoffs[ply]++;
    }

    void razor_try(const int ply) {
        if (valid_index(ply))
            counters.razor_tries[ply]++;
    }

    void razor_cutoff(const int ply) {
        if (valid_index(ply))
            counters.razor_cutoffs[ply]++;
    }

    void futility_skip(const int ply) {
        if (valid_index(ply))
            counters.futility_skips[ply]++;
    }

    void lmr_try(const int ply) {
        if (valid_index(ply))
            counters.lmr_tries[ply]++;
    }

    void lmr_research(const int ply) {
        if (valid_index(ply))
            counters.lmr_researches[ply]++;
    }

    void quiet_cutoff(const int depth) {
        if (valid_index(depth))
            counters.quiet_cutoffs[depth]++;
    }

    void quiet_malus_eligible_node(const int depth) {
        if (valid_index(depth))
            counters.quiet_malus_eligible_nodes[depth]++;
    }

    void quiet_malus_failed_quiet(const int depth) {
        if (valid_index(depth))
            counters.quiet_malus_failed_quiets[depth]++;
    }

    void quiet_malus_update(const int depth) {
        if (valid_index(depth))
            counters.quiet_malus_updates[depth]++;
    }

    Instrumentation& operator+=(const Instrumentation& other);

    const Counters& raw_counters() const { return counters; }
    std::string     str() const;

private:
    static bool valid_index(const int index) {
        return index >= 0 && index < engine::max_search_ply;
    }

    Counters counters;
};

} // namespace search
