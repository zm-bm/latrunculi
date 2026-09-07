#include "search/instrumentation.hpp"

#include <type_traits>

#include <gtest/gtest.h>

namespace search {

TEST(Instrumentation, DisabledInstrumentationIsEmptyAndNoop) {
    static_assert(std::is_empty_v<Instrumentation<false>>);

    Instrumentation<false> stats;
    Instrumentation<false> other;

    stats.node(1);
    stats.qnode(1);
    stats.beta_cutoff(1, 5);
    stats.pvs_research(1);
    stats.aspiration_fail_low();
    stats.aspiration_fail_high();
    stats.main_tt_probe(1);
    stats.main_tt_hit(1);
    stats.main_tt_cutoff(1);
    stats.q_tt_probe(1);
    stats.q_tt_hit(1);
    stats.q_tt_cutoff(1);
    stats.null_move_try(1);
    stats.null_move_cutoff(1);
    stats.razor_try(1);
    stats.razor_cutoff(1);
    stats.futility_skip(1);
    stats.lmr_try(1);
    stats.lmr_research(1);
    stats.quiet_cutoff(1);
    stats.quiet_malus_eligible_node(1);
    stats.quiet_malus_failed_quiet(1);
    stats.quiet_malus_update(1);
    stats.capture_order({.ordinal = 1}, 0, 0, 1);
    stats.lmr_history_attempt({});
    stats.lmr_history_reduced_interrupted({});
    stats.lmr_history_reduced_result({}, 0, 0);
    stats.lmr_history_research_interrupted({});
    stats.lmr_history_research_result({}, 0, 0, 1);
    stats.reset();
    stats += other;

    EXPECT_TRUE(stats.str().empty());
}

#if LATRUNCULI_SEARCH_STATS

TEST(Instrumentation, RecordsRetainedEventFamiliesAndResets) {
    Instrumentation<true> stats;

    constexpr int index = 1;
    stats.node(index);
    stats.qnode(index);
    stats.beta_cutoff(index, 1);
    stats.beta_cutoff(index, 2);
    stats.beta_cutoff(index, 4);
    stats.beta_cutoff(index, 5);
    stats.pvs_research(index);
    stats.aspiration_fail_low();
    stats.aspiration_fail_high();
    stats.main_tt_probe(index);
    stats.main_tt_hit(index);
    stats.main_tt_cutoff(index);
    stats.q_tt_probe(index);
    stats.q_tt_hit(index);
    stats.q_tt_cutoff(index);
    stats.null_move_try(index);
    stats.null_move_cutoff(index);
    stats.razor_try(index);
    stats.razor_cutoff(index);
    stats.futility_skip(index);
    stats.lmr_try(index);
    stats.lmr_research(index);
    stats.quiet_cutoff(index);
    stats.quiet_malus_eligible_node(index);
    stats.quiet_malus_failed_quiet(index);
    stats.quiet_malus_update(index);

    const auto& counters = stats.raw_counters();
    EXPECT_EQ(counters.nodes[index], 2);
    EXPECT_EQ(counters.qnodes[index], 1);
    EXPECT_EQ(counters.cutoff_index_sum[index], 12);
    EXPECT_EQ(counters.cutoff_index_1[index], 1);
    EXPECT_EQ(counters.cutoff_index_2[index], 1);
    EXPECT_EQ(counters.cutoff_index_3_4[index], 1);
    EXPECT_EQ(counters.cutoff_index_5_plus[index], 1);
    EXPECT_EQ(counters.pvs_researches[index], 1);
    EXPECT_EQ(counters.aspiration_fail_lows, 1);
    EXPECT_EQ(counters.aspiration_fail_highs, 1);
    EXPECT_EQ(counters.main_tt_probes[index], 1);
    EXPECT_EQ(counters.main_tt_hits[index], 1);
    EXPECT_EQ(counters.main_tt_cutoffs[index], 1);
    EXPECT_EQ(counters.q_tt_probes[index], 1);
    EXPECT_EQ(counters.q_tt_hits[index], 1);
    EXPECT_EQ(counters.q_tt_cutoffs[index], 1);
    EXPECT_EQ(counters.null_move_tries[index], 1);
    EXPECT_EQ(counters.null_move_cutoffs[index], 1);
    EXPECT_EQ(counters.razor_tries[index], 1);
    EXPECT_EQ(counters.razor_cutoffs[index], 1);
    EXPECT_EQ(counters.futility_skips[index], 1);
    EXPECT_EQ(counters.lmr_tries[index], 1);
    EXPECT_EQ(counters.lmr_researches[index], 1);
    EXPECT_EQ(counters.quiet_cutoffs[index], 1);
    EXPECT_EQ(counters.quiet_malus_eligible_nodes[index], 1);
    EXPECT_EQ(counters.quiet_malus_failed_quiets[index], 1);
    EXPECT_EQ(counters.quiet_malus_updates[index], 1);

    stats.reset();

    const auto& reset = stats.raw_counters();
    EXPECT_EQ(reset.nodes[index], 0);
    EXPECT_EQ(reset.cutoff_index_sum[index], 0);
    EXPECT_EQ(reset.main_tt_probes[index], 0);
    EXPECT_EQ(reset.null_move_tries[index], 0);
    EXPECT_EQ(reset.quiet_malus_updates[index], 0);
    EXPECT_EQ(reset.aspiration_fail_lows, 0);
    EXPECT_EQ(reset.aspiration_fail_highs, 0);
}

TEST(Instrumentation, IgnoresOutOfRangeIndices) {
    Instrumentation<true> stats;

    stats.node(-1);
    stats.node(engine::max_search_ply);
    stats.beta_cutoff(-1, 1);
    stats.beta_cutoff(engine::max_search_ply, 1);
    stats.quiet_malus_update(-1);
    stats.quiet_malus_update(engine::max_search_ply);

    const auto& counters = stats.raw_counters();
    EXPECT_EQ(counters.nodes.front(), 0);
    EXPECT_EQ(counters.nodes.back(), 0);
    EXPECT_EQ(counters.cutoff_index_1.front(), 0);
    EXPECT_EQ(counters.cutoff_index_1.back(), 0);
    EXPECT_EQ(counters.quiet_malus_updates.front(), 0);
    EXPECT_EQ(counters.quiet_malus_updates.back(), 0);
}

TEST(Instrumentation, RecordsCaptureOrderingOutcomesBucketsAndExactOrdinals) {
    Instrumentation<true> stats;

    const auto observation = [](const int ordinal) {
        return CaptureOrderObservation{
            .context = CaptureOrderContext::Main,
            .node    = CaptureOrderNode::NonPv,
            .stage   = CaptureOrderStage::ExactSeeGood,
            .move    = CaptureOrderMove::OrdinaryCapture,
            .ordinal = ordinal,
        };
    };

    stats.capture_order(observation(0), 100, 0, 100);
    stats.capture_order(observation(1), 0, 0, 100);
    stats.capture_order(observation(2), 1, 0, 100);
    stats.capture_order(observation(3), 100, 0, 100);
    stats.capture_order(observation(4), -1, 0, 100);
    stats.capture_order(observation(5), 1, 0, 100);
    stats.capture_order(observation(8), 100, 0, 100);
    stats.capture_order(observation(9), 0, 0, 100);

    const auto cell = [&](const CaptureOrderBucket bucket) -> const CaptureOrderCell& {
        return stats.raw_counters()
            .capture_order[capture_order_index(CaptureOrderContext::Main,
                                               CaptureOrderNode::NonPv,
                                               CaptureOrderStage::ExactSeeGood,
                                               CaptureOrderMove::OrdinaryCapture,
                                               bucket)];
    };

    EXPECT_EQ(cell(CaptureOrderBucket::One).fail_lows, 1U);
    EXPECT_EQ(cell(CaptureOrderBucket::Two).alpha_raises, 1U);
    EXPECT_EQ(cell(CaptureOrderBucket::Two).success_ordinal_sum, 2U);
    EXPECT_EQ(cell(CaptureOrderBucket::ThreeFour).fail_lows, 1U);
    EXPECT_EQ(cell(CaptureOrderBucket::ThreeFour).beta_cutoffs, 1U);
    EXPECT_EQ(cell(CaptureOrderBucket::ThreeFour).success_ordinal_sum, 3U);
    EXPECT_EQ(cell(CaptureOrderBucket::FiveEight).alpha_raises, 1U);
    EXPECT_EQ(cell(CaptureOrderBucket::FiveEight).beta_cutoffs, 1U);
    EXPECT_EQ(cell(CaptureOrderBucket::FiveEight).success_ordinal_sum, 13U);
    EXPECT_EQ(cell(CaptureOrderBucket::NinePlus).fail_lows, 1U);

    stats.reset();
    EXPECT_EQ(cell(CaptureOrderBucket::One).fail_lows, 0U);
    EXPECT_EQ(cell(CaptureOrderBucket::FiveEight).success_ordinal_sum, 0U);
}

TEST(Instrumentation, RecordsLmrHistoryBucketsAndOutcomes) {
    Instrumentation<true> stats;
    const LmrObservation  observation{
         .node          = LmrNode::Pv,
         .move_class    = LmrMoveClass::Quiet,
         .depth         = 6,
         .reduction     = 2,
         .history_score = 1024,
    };

    stats.lmr_history_attempt(observation);
    stats.lmr_history_reduced_interrupted(observation);

    stats.lmr_history_attempt(observation);
    stats.lmr_history_reduced_result(observation, 0, 0);

    stats.lmr_history_attempt(observation);
    stats.lmr_history_reduced_result(observation, 1, 0);
    stats.lmr_history_research_interrupted(observation);

    stats.lmr_history_attempt(observation);
    stats.lmr_history_reduced_result(observation, 1, 0);
    stats.lmr_history_research_result(observation, 0, 0, 100);

    stats.lmr_history_attempt(observation);
    stats.lmr_history_reduced_result(observation, 1, 0);
    stats.lmr_history_research_result(observation, 50, 0, 100);

    stats.lmr_history_attempt(observation);
    stats.lmr_history_reduced_result(observation, 1, 0);
    stats.lmr_history_research_result(observation, 100, 0, 100);

    const std::size_t index = lmr_history_index(
        LmrNode::Pv, LmrMoveClass::Quiet, 6, LmrReductionBucket::Two, LmrHistoryBucket::Ge1024);
    const auto& cell = stats.raw_counters().lmr_history[index];
    EXPECT_EQ(cell.attempts, 6U);
    EXPECT_EQ(cell.reduced_interrupted, 1U);
    EXPECT_EQ(cell.reduced_fail_lows, 1U);
    EXPECT_EQ(cell.reduced_alpha_raises, 4U);
    EXPECT_EQ(cell.research_interrupted, 1U);
    EXPECT_EQ(cell.research_refuted, 1U);
    EXPECT_EQ(cell.research_alpha_raises, 1U);
    EXPECT_EQ(cell.research_cutoffs, 1U);

    EXPECT_EQ(lmr_history_bucket(LmrMoveClass::Quiet, -1024), LmrHistoryBucket::LeNegative1024);
    EXPECT_EQ(lmr_history_bucket(LmrMoveClass::Quiet, -1023), LmrHistoryBucket::Negative);
    EXPECT_EQ(lmr_history_bucket(LmrMoveClass::Quiet, -1), LmrHistoryBucket::Negative);
    EXPECT_EQ(lmr_history_bucket(LmrMoveClass::Quiet, 0), LmrHistoryBucket::Zero);
    EXPECT_EQ(lmr_history_bucket(LmrMoveClass::Quiet, 1), LmrHistoryBucket::Positive);
    EXPECT_EQ(lmr_history_bucket(LmrMoveClass::Quiet, 1023), LmrHistoryBucket::Positive);
    EXPECT_EQ(lmr_history_bucket(LmrMoveClass::Quiet, 1024), LmrHistoryBucket::Ge1024);
    EXPECT_EQ(lmr_history_bucket(LmrMoveClass::Noisy, 1024), LmrHistoryBucket::NotApplicable);
    EXPECT_EQ(lmr_reduction_bucket(1), LmrReductionBucket::One);
    EXPECT_EQ(lmr_reduction_bucket(2), LmrReductionBucket::Two);
    EXPECT_EQ(lmr_reduction_bucket(3), LmrReductionBucket::ThreePlus);

    stats.reset();
    EXPECT_EQ(stats.raw_counters().lmr_history[index].attempts, 0U);
}

TEST(Instrumentation, FormatsCappedLmrVerifierControlSamples) {
    LmrVerifier verifier;
    verifier.reset(std::nullopt);
    EXPECT_TRUE(verifier.str().empty());

    verifier.reset(0);
    const LmrVerifierObservation observation{
        .node          = LmrNode::Pv,
        .parent_key    = 0x1234,
        .move          = Move(A1, A2),
        .ply           = 2,
        .depth         = 6,
        .reduction     = 2,
        .history_score = 1024,
        .alpha         = 10,
        .beta          = 11,
        .reduced_value = 10,
        .nodes         = 500,
    };

    for (std::uint64_t occurrence = 1; occurrence <= LmrVerifier::max_occurrence; ++occurrence)
        EXPECT_FALSE(verifier.observe_fail_low(observation));
    EXPECT_FALSE(verifier.observe_fail_low(observation));

    const std::string report = verifier.str();
    EXPECT_NE(report.find("LmrVerifier: schema=1 mode=control target=0\n"), std::string::npos);
    EXPECT_NE(report.find("LmrVerifierSample: occurrence=64 "), std::string::npos);
    EXPECT_NE(report.find("LmrVerifierSample: occurrence=2048 "), std::string::npos);
    EXPECT_EQ(report.find("LmrVerifierResult:"), std::string::npos);
    EXPECT_NE(
        report.find("LmrVerifierTotal: schema=1 samples=32 target=0 reached=0 contaminated=0\n"),
        std::string::npos);

    verifier.reset(std::nullopt);
    EXPECT_TRUE(verifier.str().empty());
}

TEST(Instrumentation, ActivatesAndClassifiesOneLmrVerifierTarget) {
    LmrVerifier verifier;
    verifier.reset(128);

    LmrVerifierObservation observation{
        .node          = LmrNode::Pv,
        .parent_key    = 0x1234,
        .move          = Move(A1, A2),
        .ply           = 2,
        .depth         = 6,
        .reduction     = 2,
        .history_score = 1024,
        .alpha         = 10,
        .beta          = 11,
        .reduced_value = 10,
        .nodes         = 500,
    };

    LmrVerifierObservation ineligible = observation;
    ineligible.history_score          = 1023;
    EXPECT_FALSE(verifier.observe_fail_low(ineligible));
    ineligible               = observation;
    ineligible.reduced_value = 11;
    EXPECT_FALSE(verifier.observe_fail_low(ineligible));

    for (std::uint64_t occurrence = 1; occurrence < 128; ++occurrence)
        EXPECT_FALSE(verifier.observe_fail_low(observation));
    EXPECT_TRUE(verifier.observe_fail_low(observation));
    EXPECT_FALSE(verifier.observe_fail_low(observation));

    verifier.complete(11, 750);
    EXPECT_EQ(verifier.str(), R"(LmrVerifier: schema=1 mode=active target=128
LmrVerifierSample: occurrence=64 parent-key=0000000000001234 move=a1a2 node=pv ply=2 depth=6 reduction=2 history=1024 alpha=10 beta=11 reduced-value=10 nodes=500
LmrVerifierSample: occurrence=128 parent-key=0000000000001234 move=a1a2 node=pv ply=2 depth=6 reduction=2 history=1024 alpha=10 beta=11 reduced-value=10 nodes=500
LmrVerifierResult: occurrence=128 full-value=11 nodes-before=500 nodes-after=750 outcome=false-fail-low
LmrVerifierTotal: schema=1 samples=2 target=128 reached=1 contaminated=1
)");

    verifier.reset(64);
    for (std::uint64_t occurrence = 1; occurrence < 64; ++occurrence)
        EXPECT_FALSE(verifier.observe_fail_low(observation));
    EXPECT_TRUE(verifier.observe_fail_low(observation));
    verifier.complete(10, 600);
    EXPECT_NE(verifier.str().find("outcome=confirmed\n"), std::string::npos);

    verifier.reset(128);
    for (std::uint64_t occurrence = 1; occurrence <= 64; ++occurrence)
        EXPECT_FALSE(verifier.observe_fail_low(observation));
    EXPECT_NE(verifier.str().find(
                  "LmrVerifierTotal: schema=1 samples=1 target=128 reached=0 contaminated=0\n"),
              std::string::npos);
}

TEST(Instrumentation, AggregatesCounters) {
    Counters first;
    Counters second;

    first.nodes[1]                      = 10;
    first.qnodes[1]                     = 4;
    first.cutoff_index_sum[1]           = 3;
    first.cutoff_index_1[1]             = 2;
    first.cutoff_index_2[1]             = 1;
    first.cutoff_index_3_4[1]           = 2;
    first.cutoff_index_5_plus[1]        = 3;
    first.pvs_researches[1]             = 1;
    first.main_tt_probes[1]             = 4;
    first.main_tt_hits[1]               = 3;
    first.main_tt_cutoffs[1]            = 2;
    first.q_tt_probes[1]                = 5;
    first.q_tt_hits[1]                  = 4;
    first.q_tt_cutoffs[1]               = 2;
    first.null_move_tries[1]            = 6;
    first.null_move_cutoffs[1]          = 3;
    first.razor_tries[1]                = 4;
    first.razor_cutoffs[1]              = 2;
    first.futility_skips[1]             = 7;
    first.lmr_tries[1]                  = 8;
    first.lmr_researches[1]             = 4;
    first.quiet_cutoffs[1]              = 3;
    first.quiet_malus_eligible_nodes[1] = 4;
    first.quiet_malus_failed_quiets[1]  = 5;
    first.quiet_malus_updates[1]        = 6;
    first.aspiration_fail_lows          = 1;
    first.aspiration_fail_highs         = 2;

    second.nodes[1]                      = 5;
    second.qnodes[1]                     = 3;
    second.cutoff_index_sum[1]           = 9;
    second.cutoff_index_1[1]             = 3;
    second.cutoff_index_2[1]             = 2;
    second.cutoff_index_3_4[1]           = 4;
    second.cutoff_index_5_plus[1]        = 6;
    second.pvs_researches[1]             = 2;
    second.main_tt_probes[1]             = 6;
    second.main_tt_hits[1]               = 4;
    second.main_tt_cutoffs[1]            = 3;
    second.q_tt_probes[1]                = 7;
    second.q_tt_hits[1]                  = 5;
    second.q_tt_cutoffs[1]               = 3;
    second.null_move_tries[1]            = 8;
    second.null_move_cutoffs[1]          = 5;
    second.razor_tries[1]                = 9;
    second.razor_cutoffs[1]              = 6;
    second.futility_skips[1]             = 11;
    second.lmr_tries[1]                  = 12;
    second.lmr_researches[1]             = 3;
    second.quiet_cutoffs[1]              = 7;
    second.quiet_malus_eligible_nodes[1] = 8;
    second.quiet_malus_failed_quiets[1]  = 9;
    second.quiet_malus_updates[1]        = 10;
    second.aspiration_fail_lows          = 4;
    second.aspiration_fail_highs         = 5;

    constexpr std::size_t capture_index = capture_order_index(CaptureOrderContext::Main,
                                                              CaptureOrderNode::Pv,
                                                              CaptureOrderStage::ExactSeeBad,
                                                              CaptureOrderMove::OrdinaryCapture,
                                                              CaptureOrderBucket::Two);
    first.capture_order[capture_index]  = {
         .fail_lows = 2, .alpha_raises = 3, .beta_cutoffs = 4, .success_ordinal_sum = 14};
    second.capture_order[capture_index] = {
        .fail_lows = 5, .alpha_raises = 6, .beta_cutoffs = 7, .success_ordinal_sum = 26};

    constexpr std::size_t lmr_index = lmr_history_index(LmrNode::NonPv,
                                                        LmrMoveClass::Noisy,
                                                        5,
                                                        LmrReductionBucket::One,
                                                        LmrHistoryBucket::NotApplicable);
    first.lmr_history[lmr_index]    = {
           .attempts = 4, .reduced_fail_lows = 3, .reduced_alpha_raises = 1, .research_cutoffs = 1};
    second.lmr_history[lmr_index] = {.attempts             = 6,
                                     .reduced_interrupted  = 1,
                                     .reduced_fail_lows    = 4,
                                     .reduced_alpha_raises = 1,
                                     .research_refuted     = 1};

    Instrumentation<true> total{first};
    total += Instrumentation<true>{second};

    const auto& counters = total.raw_counters();
    EXPECT_EQ(counters.nodes[1], 15);
    EXPECT_EQ(counters.qnodes[1], 7);
    EXPECT_EQ(counters.cutoff_index_sum[1], 12);
    EXPECT_EQ(counters.cutoff_index_1[1], 5);
    EXPECT_EQ(counters.cutoff_index_2[1], 3);
    EXPECT_EQ(counters.cutoff_index_3_4[1], 6);
    EXPECT_EQ(counters.cutoff_index_5_plus[1], 9);
    EXPECT_EQ(counters.pvs_researches[1], 3);
    EXPECT_EQ(counters.main_tt_probes[1], 10);
    EXPECT_EQ(counters.main_tt_hits[1], 7);
    EXPECT_EQ(counters.main_tt_cutoffs[1], 5);
    EXPECT_EQ(counters.q_tt_probes[1], 12);
    EXPECT_EQ(counters.q_tt_hits[1], 9);
    EXPECT_EQ(counters.q_tt_cutoffs[1], 5);
    EXPECT_EQ(counters.null_move_tries[1], 14);
    EXPECT_EQ(counters.null_move_cutoffs[1], 8);
    EXPECT_EQ(counters.razor_tries[1], 13);
    EXPECT_EQ(counters.razor_cutoffs[1], 8);
    EXPECT_EQ(counters.futility_skips[1], 18);
    EXPECT_EQ(counters.lmr_tries[1], 20);
    EXPECT_EQ(counters.lmr_researches[1], 7);
    EXPECT_EQ(counters.quiet_cutoffs[1], 10);
    EXPECT_EQ(counters.quiet_malus_eligible_nodes[1], 12);
    EXPECT_EQ(counters.quiet_malus_failed_quiets[1], 14);
    EXPECT_EQ(counters.quiet_malus_updates[1], 16);
    EXPECT_EQ(counters.aspiration_fail_lows, 5);
    EXPECT_EQ(counters.aspiration_fail_highs, 7);
    EXPECT_EQ(counters.capture_order[capture_index].fail_lows, 7U);
    EXPECT_EQ(counters.capture_order[capture_index].alpha_raises, 9U);
    EXPECT_EQ(counters.capture_order[capture_index].beta_cutoffs, 11U);
    EXPECT_EQ(counters.capture_order[capture_index].success_ordinal_sum, 40U);
    EXPECT_EQ(counters.lmr_history[lmr_index].attempts, 10U);
    EXPECT_EQ(counters.lmr_history[lmr_index].reduced_interrupted, 1U);
    EXPECT_EQ(counters.lmr_history[lmr_index].reduced_fail_lows, 7U);
    EXPECT_EQ(counters.lmr_history[lmr_index].reduced_alpha_raises, 2U);
    EXPECT_EQ(counters.lmr_history[lmr_index].research_refuted, 1U);
    EXPECT_EQ(counters.lmr_history[lmr_index].research_cutoffs, 1U);
}

TEST(Instrumentation, FormatsStableDiagnostics) {
    Counters counters;
    counters.aspiration_fail_lows          = 1;
    counters.aspiration_fail_highs         = 2;
    counters.nodes[1]                      = 100;
    counters.nodes[2]                      = 200;
    counters.qnodes[1]                     = 50;
    counters.qnodes[2]                     = 100;
    counters.cutoff_index_sum[1]           = 170;
    counters.cutoff_index_1[1]             = 40;
    counters.cutoff_index_2[1]             = 20;
    counters.cutoff_index_3_4[1]           = 10;
    counters.cutoff_index_5_plus[1]        = 10;
    counters.cutoff_index_sum[2]           = 325;
    counters.cutoff_index_1[2]             = 75;
    counters.cutoff_index_2[2]             = 35;
    counters.cutoff_index_3_4[2]           = 20;
    counters.cutoff_index_5_plus[2]        = 20;
    counters.pvs_researches[1]             = 7;
    counters.main_tt_probes[1]             = 60;
    counters.main_tt_hits[1]               = 30;
    counters.main_tt_cutoffs[1]            = 20;
    counters.q_tt_probes[1]                = 40;
    counters.q_tt_hits[1]                  = 10;
    counters.q_tt_cutoffs[1]               = 5;
    counters.null_move_tries[1]            = 6;
    counters.null_move_cutoffs[1]          = 3;
    counters.null_move_tries[2]            = 4;
    counters.null_move_cutoffs[2]          = 1;
    counters.razor_tries[1]                = 7;
    counters.razor_cutoffs[1]              = 2;
    counters.razor_tries[2]                = 3;
    counters.razor_cutoffs[2]              = 2;
    counters.futility_skips[1]             = 5;
    counters.futility_skips[2]             = 6;
    counters.lmr_tries[1]                  = 8;
    counters.lmr_researches[1]             = 2;
    counters.lmr_tries[2]                  = 12;
    counters.lmr_researches[2]             = 3;
    counters.quiet_cutoffs[4]              = 6;
    counters.quiet_malus_eligible_nodes[4] = 7;
    counters.quiet_malus_failed_quiets[4]  = 8;
    counters.quiet_malus_updates[4]        = 5;

    counters.capture_order[capture_order_index(CaptureOrderContext::Main,
                                               CaptureOrderNode::Pv,
                                               CaptureOrderStage::Tt,
                                               CaptureOrderMove::Quiet,
                                               CaptureOrderBucket::One)] = {
        .fail_lows = 2, .alpha_raises = 1, .beta_cutoffs = 1, .success_ordinal_sum = 2};
    counters.capture_order[capture_order_index(CaptureOrderContext::Qsearch,
                                               CaptureOrderNode::NonPv,
                                               CaptureOrderStage::ExactSeeGood,
                                               CaptureOrderMove::OrdinaryCapture,
                                               CaptureOrderBucket::ThreeFour)] = {
        .fail_lows = 3, .alpha_raises = 2, .beta_cutoffs = 1, .success_ordinal_sum = 10};

    counters.lmr_history[lmr_history_index(
        LmrNode::Pv, LmrMoveClass::Quiet, 4, LmrReductionBucket::One, LmrHistoryBucket::Positive)] =
        {
            .attempts             = 4,
            .reduced_fail_lows    = 2,
            .reduced_alpha_raises = 2,
            .research_refuted     = 1,
            .research_cutoffs     = 1,
        };

    const Instrumentation<true> stats{counters};

    EXPECT_EQ(stats.str(), R"(
Aspiration: fail-low=1 fail-high=2 re-searches=3
NullMove: tries=10 cutoffs=4 cutoff-rate=40.0%
RazorFutility: razor-tries=10 razor-cutoffs=4 razor-cutoff-rate=40.0% futility-skips=11
LMR: tries=20 re-searches=5 re-search-rate=25.0%
QuietHistory: quiet-cutoffs=6 malus-eligible=7 failed-quiets=8 malus-updates=5
 QH D |       Cutoffs |      Eligible |   FailedQuiet |   MalusUpdate
    4 |             6 |             7 |             8 |             5
  Ply |     Nodes (QNode%) |  Cutoffs (Early%/Late%) |      CutIdx Avg/1/2/3-4/5+% | PVS Re | MainTT Hit/Cut% |  QTT Hit/Cut% |   EBF / Cumul
    1 |       100 ( 50.0%) |       80 ( 50.0/ 50.0%) |  2.1 /  50.0/ 25.0/ 12.5/ 12.5% |      7 |  50.0/ 66.7% |  25.0/ 50.0% |   0.0 / 100.0
    2 |       200 ( 50.0%) |      150 ( 50.0/ 50.0%) |  2.2 /  50.0/ 23.3/ 13.3/ 13.3% |      0 |   0.0/  0.0% |   0.0/  0.0% |   2.0 /  14.1
CaptureOrdering: schema=1
CaptureOrderingCell: context=main node=pv stage=tt move=quiet ordinal=1 attempts=4 fail-low=2 alpha-raise=1 beta-cutoff=1 success-ordinal-sum=2
CaptureOrderingCell: context=qsearch node=nonpv stage=exact-see-good move=ordinary-capture ordinal=3-4 attempts=6 fail-low=3 alpha-raise=2 beta-cutoff=1 success-ordinal-sum=10
CaptureOrderingTotal: schema=1 cells=2 attempts=10 fail-low=5 alpha-raise=3 beta-cutoff=2 success-ordinal-sum=12
LmrHistory: schema=1
LmrHistoryCell: node=pv move=quiet depth=4 reduction=1 history=positive attempts=4 reduced-interrupted=0 reduced-fail-low=2 reduced-alpha-raise=2 research-interrupted=0 research-refuted=1 research-alpha-raise=0 research-cutoff=1
LmrHistoryTotal: schema=1 cells=1 attempts=4 reduced-interrupted=0 reduced-fail-low=2 reduced-alpha-raise=2 research-interrupted=0 research-refuted=1 research-alpha-raise=0 research-cutoff=1
)");
}

#endif

} // namespace search
