#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "core/constants.hpp"
#include "core/move.hpp"

#ifndef LATRUNCULI_SEARCH_STATS
#define LATRUNCULI_SEARCH_STATS 0
#endif

namespace search {

constexpr bool stats_enabled = LATRUNCULI_SEARCH_STATS;

enum class CaptureOrderContext : std::uint8_t { Main, Qsearch, Count };
enum class CaptureOrderNode : std::uint8_t { Pv, NonPv, Count };
enum class CaptureOrderStage : std::uint8_t {
    Tt,
    QsearchTtSeeBad,
    Evasion,
    Promotion,
    ExactSeeGood,
    ExactSeeBad,
    Count,
};
enum class CaptureOrderMove : std::uint8_t { OrdinaryCapture, EnPassant, Promotion, Quiet, Count };
enum class CaptureOrderBucket : std::uint8_t { One, Two, ThreeFour, FiveEight, NinePlus, Count };

inline constexpr std::size_t capture_order_context_count =
    static_cast<std::size_t>(CaptureOrderContext::Count);
inline constexpr std::size_t capture_order_node_count =
    static_cast<std::size_t>(CaptureOrderNode::Count);
inline constexpr std::size_t capture_order_stage_count =
    static_cast<std::size_t>(CaptureOrderStage::Count);
inline constexpr std::size_t capture_order_move_count =
    static_cast<std::size_t>(CaptureOrderMove::Count);
inline constexpr std::size_t capture_order_bucket_count =
    static_cast<std::size_t>(CaptureOrderBucket::Count);
inline constexpr std::size_t capture_order_cell_count =
    capture_order_context_count * capture_order_node_count * capture_order_stage_count
    * capture_order_move_count * capture_order_bucket_count;

struct CaptureOrderObservation {
    CaptureOrderContext context{CaptureOrderContext::Main};
    CaptureOrderNode    node{CaptureOrderNode::NonPv};
    CaptureOrderStage   stage{CaptureOrderStage::Tt};
    CaptureOrderMove    move{CaptureOrderMove::Quiet};
    int                 ordinal{0};
};

struct CaptureOrderCell {
    std::uint64_t fail_lows{0};
    std::uint64_t alpha_raises{0};
    std::uint64_t beta_cutoffs{0};
    std::uint64_t success_ordinal_sum{0};
};

enum class LmrNode : std::uint8_t { Pv, NonPv, Count };
enum class LmrMoveClass : std::uint8_t { Quiet, Noisy, Count };
enum class LmrReductionBucket : std::uint8_t { One, Two, ThreePlus, Count };
enum class LmrHistoryBucket : std::uint8_t {
    LeNegative1024,
    Negative,
    Zero,
    Positive,
    Ge1024,
    NotApplicable,
    Count,
};

inline constexpr std::size_t lmr_node_count       = static_cast<std::size_t>(LmrNode::Count);
inline constexpr std::size_t lmr_move_class_count = static_cast<std::size_t>(LmrMoveClass::Count);
inline constexpr std::size_t lmr_depth_count      = engine::max_search_depth + 1;
inline constexpr std::size_t lmr_reduction_bucket_count =
    static_cast<std::size_t>(LmrReductionBucket::Count);
inline constexpr std::size_t lmr_history_bucket_count =
    static_cast<std::size_t>(LmrHistoryBucket::Count);
inline constexpr std::size_t lmr_history_cell_count = lmr_node_count * lmr_move_class_count
                                                    * lmr_depth_count * lmr_reduction_bucket_count
                                                    * lmr_history_bucket_count;

struct LmrObservation {
    LmrNode      node{LmrNode::NonPv};
    LmrMoveClass move_class{LmrMoveClass::Quiet};
    int          depth{0};
    int          reduction{0};
    int          history_score{0};
};

struct LmrHistoryCell {
    std::uint64_t attempts{0};
    std::uint64_t reduced_interrupted{0};
    std::uint64_t reduced_fail_lows{0};
    std::uint64_t reduced_alpha_raises{0};
    std::uint64_t research_interrupted{0};
    std::uint64_t research_refuted{0};
    std::uint64_t research_alpha_raises{0};
    std::uint64_t research_cutoffs{0};
};

#if LATRUNCULI_SEARCH_STATS

struct LmrVerifierObservation {
    LmrNode     node{LmrNode::NonPv};
    PositionKey parent_key{0};
    Move        move{NULL_MOVE};
    int         ply{0};
    int         depth{0};
    int         reduction{0};
    int         history_score{0};
    EvalValue   alpha{0};
    EvalValue   beta{0};
    EvalValue   reduced_value{0};
    NodeCount   nodes{0};
};

class LmrVerifier {
public:
    static constexpr std::uint64_t occurrence_stride = 64;
    static constexpr std::uint64_t max_occurrence    = 2048;
    static constexpr std::size_t   max_samples       = max_occurrence / occurrence_stride;

    void reset(std::optional<std::uint64_t> target);

    // Returns true only when this observation is the configured active target.
    bool observe_fail_low(const LmrVerifierObservation& observation);
    void complete(EvalValue full_value, NodeCount nodes_after);

    [[nodiscard]] std::string str() const;

private:
    struct Sample : LmrVerifierObservation {
        std::uint64_t occurrence{0};
    };

    std::optional<std::uint64_t>    target;
    std::uint64_t                   occurrences{0};
    std::array<Sample, max_samples> samples{};
    std::size_t                     sample_count{0};
    bool                            suppressed{false};
    bool                            reached{false};
    bool                            contaminated{false};
    EvalValue                       full_value{0};
    NodeCount                       nodes_after{0};
};

#endif

constexpr CaptureOrderBucket capture_order_bucket(const int ordinal) {
    if (ordinal <= 1)
        return CaptureOrderBucket::One;
    if (ordinal == 2)
        return CaptureOrderBucket::Two;
    if (ordinal <= 4)
        return CaptureOrderBucket::ThreeFour;
    if (ordinal <= 8)
        return CaptureOrderBucket::FiveEight;
    return CaptureOrderBucket::NinePlus;
}

constexpr std::size_t capture_order_index(CaptureOrderContext context,
                                          CaptureOrderNode    node,
                                          CaptureOrderStage   stage,
                                          CaptureOrderMove    move,
                                          CaptureOrderBucket  bucket) {
    std::size_t index = static_cast<std::size_t>(context);
    index             = index * capture_order_node_count + static_cast<std::size_t>(node);
    index             = index * capture_order_stage_count + static_cast<std::size_t>(stage);
    index             = index * capture_order_move_count + static_cast<std::size_t>(move);
    return index * capture_order_bucket_count + static_cast<std::size_t>(bucket);
}

constexpr LmrReductionBucket lmr_reduction_bucket(const int reduction) {
    if (reduction <= 1)
        return LmrReductionBucket::One;
    if (reduction == 2)
        return LmrReductionBucket::Two;
    return LmrReductionBucket::ThreePlus;
}

constexpr LmrHistoryBucket lmr_history_bucket(const LmrMoveClass move_class,
                                              const int          history_score) {
    if (move_class == LmrMoveClass::Noisy)
        return LmrHistoryBucket::NotApplicable;
    if (history_score <= -1024)
        return LmrHistoryBucket::LeNegative1024;
    if (history_score < 0)
        return LmrHistoryBucket::Negative;
    if (history_score == 0)
        return LmrHistoryBucket::Zero;
    if (history_score < 1024)
        return LmrHistoryBucket::Positive;
    return LmrHistoryBucket::Ge1024;
}

constexpr std::size_t lmr_history_index(const LmrNode            node,
                                        const LmrMoveClass       move_class,
                                        const int                depth,
                                        const LmrReductionBucket reduction,
                                        const LmrHistoryBucket   history) {
    std::size_t index = static_cast<std::size_t>(node);
    index             = index * lmr_move_class_count + static_cast<std::size_t>(move_class);
    index             = index * lmr_depth_count + static_cast<std::size_t>(depth);
    index             = index * lmr_reduction_bucket_count + static_cast<std::size_t>(reduction);
    return index * lmr_history_bucket_count + static_cast<std::size_t>(history);
}

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

    std::array<CaptureOrderCell, capture_order_cell_count> capture_order{};
    std::array<LmrHistoryCell, lmr_history_cell_count>     lmr_history{};

    std::uint64_t aspiration_fail_lows{0};
    std::uint64_t aspiration_fail_highs{0};
};

template <bool Enable = stats_enabled>
class Instrumentation;

template <>
class Instrumentation<false> {
public:
    void reset() {}
    void node(int) {}
    void qnode(int) {}
    void beta_cutoff(int, int) {}
    void pvs_research(int) {}
    void aspiration_fail_low() {}
    void aspiration_fail_high() {}
    void main_tt_probe(int) {}
    void main_tt_hit(int) {}
    void main_tt_cutoff(int) {}
    void q_tt_probe(int) {}
    void q_tt_hit(int) {}
    void q_tt_cutoff(int) {}
    void null_move_try(int) {}
    void null_move_cutoff(int) {}
    void razor_try(int) {}
    void razor_cutoff(int) {}
    void futility_skip(int) {}
    void lmr_try(int) {}
    void lmr_research(int) {}
    void quiet_cutoff(int) {}
    void quiet_malus_eligible_node(int) {}
    void quiet_malus_failed_quiet(int) {}
    void quiet_malus_update(int) {}
    void capture_order(const CaptureOrderObservation&, EvalValue, EvalValue, EvalValue) {}
    void lmr_history_attempt(const LmrObservation&) {}
    void lmr_history_reduced_interrupted(const LmrObservation&) {}
    void lmr_history_reduced_result(const LmrObservation&, EvalValue, EvalValue) {}
    void lmr_history_research_interrupted(const LmrObservation&) {}
    void lmr_history_research_result(const LmrObservation&, EvalValue, EvalValue, EvalValue) {}
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

    void capture_order(const CaptureOrderObservation& observation,
                       const EvalValue                value,
                       const EvalValue                alpha_before_move,
                       const EvalValue                beta) {
        if (observation.ordinal <= 0)
            return;

        CaptureOrderCell& cell =
            counters.capture_order[capture_order_index(observation.context,
                                                       observation.node,
                                                       observation.stage,
                                                       observation.move,
                                                       capture_order_bucket(observation.ordinal))];
        if (value >= beta) {
            cell.beta_cutoffs++;
            cell.success_ordinal_sum += static_cast<std::uint64_t>(observation.ordinal);
        } else if (value > alpha_before_move) {
            cell.alpha_raises++;
            cell.success_ordinal_sum += static_cast<std::uint64_t>(observation.ordinal);
        } else {
            cell.fail_lows++;
        }
    }

    void lmr_history_attempt(const LmrObservation& observation) {
        if (LmrHistoryCell* cell = lmr_cell(observation))
            cell->attempts++;
    }

    void lmr_history_reduced_interrupted(const LmrObservation& observation) {
        if (LmrHistoryCell* cell = lmr_cell(observation))
            cell->reduced_interrupted++;
    }

    void lmr_history_reduced_result(const LmrObservation& observation,
                                    const EvalValue       value,
                                    const EvalValue       alpha_before_move) {
        if (LmrHistoryCell* cell = lmr_cell(observation)) {
            if (value > alpha_before_move)
                cell->reduced_alpha_raises++;
            else
                cell->reduced_fail_lows++;
        }
    }

    void lmr_history_research_interrupted(const LmrObservation& observation) {
        if (LmrHistoryCell* cell = lmr_cell(observation))
            cell->research_interrupted++;
    }

    void lmr_history_research_result(const LmrObservation& observation,
                                     const EvalValue       value,
                                     const EvalValue       alpha_before_move,
                                     const EvalValue       beta) {
        if (LmrHistoryCell* cell = lmr_cell(observation)) {
            if (value >= beta)
                cell->research_cutoffs++;
            else if (value > alpha_before_move)
                cell->research_alpha_raises++;
            else
                cell->research_refuted++;
        }
    }

    Instrumentation& operator+=(const Instrumentation& other);

    const Counters& raw_counters() const { return counters; }
    std::string     str() const;

private:
    static bool valid_index(const int index) {
        return index >= 0 && index < engine::max_search_ply;
    }

    LmrHistoryCell* lmr_cell(const LmrObservation& observation) {
        if (observation.depth < 3 || observation.depth > engine::max_search_depth
            || observation.reduction <= 0)
            return nullptr;

        return &counters.lmr_history[lmr_history_index(
            observation.node,
            observation.move_class,
            observation.depth,
            lmr_reduction_bucket(observation.reduction),
            lmr_history_bucket(observation.move_class, observation.history_score))];
    }

    Counters counters;
};

} // namespace search
