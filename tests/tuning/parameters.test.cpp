#include "tuning/parameters.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

#include "board/see_values.hpp"
#include "movegen/moves.hpp"
#include "search/history_learning.hpp"
#include "support/engine_test_fixture.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

static_assert(!std::is_constructible_v<movegen::MainSearchMoves,
                                       const Board&,
                                       const search::History&,
                                       search::OrderingContext&&,
                                       const tuning::Parameters&>);
static_assert(!std::is_constructible_v<search::HistoryLearning,
                                       const Board&,
                                       search::History&,
                                       search::OrderingContext&&,
                                       int,
                                       search::Instrumentation<>&,
                                       const tuning::Parameters&>);

TEST(TuningTest, RegistryAndBuildMode) {
    EXPECT_EQ(std::ranges::count_if(tuning::definitions(), [](auto p) { return p.search; }), 28);
    for (const auto& p : tuning::definitions()) {
        EXPECT_LE(p.minimum, p.initial) << p.name;
        EXPECT_GE(p.maximum, p.initial) << p.name;
    }
    std::ostringstream output;
#if LATRUNCULI_TUNING
    tuning::export_spsa(output, "search");
    EXPECT_EQ(std::ranges::count(output.str(), '\n'), 27);
    EXPECT_FALSE(output.str().ends_with('\n'));
    EXPECT_NE(output.str().find("SeeKnight, int, 350, 250, 375, 6.25, 0.002"), std::string::npos);
    EXPECT_NE(tuning::uci_options().find("LmrPvScale type string default 0.7"), std::string::npos);
    EXPECT_THROW(tuning::export_spsa(output, "unknown"), std::invalid_argument);
#else
    EXPECT_TRUE(tuning::uci_options().empty());
    EXPECT_THROW(tuning::export_spsa(output, "search"), std::runtime_error);
    tuning::Parameters p;
    EXPECT_FALSE(tuning::set(p, "SeeKnight", "375"));
    static_assert(tuning::Parameters::SeeKnight == 350);
#endif
}

#if LATRUNCULI_TUNING
TEST(TuningTest, EveryOptionAcceptsBoundsAndRejectsInvalidValues) {
    for (const auto& definition : tuning::definitions()) {
        SCOPED_TRACE(definition.name);
        tuning::Parameters p;
        for (double value : {definition.minimum, definition.initial, definition.maximum})
            ASSERT_TRUE(tuning::set(p, definition.name, std::format("{}", value)));
        for (const auto& value : {"", "nan", "inf", "-inf", "1e999", "12 extra"})
            EXPECT_THROW(tuning::set(p, definition.name, value), std::exception);
        EXPECT_THROW(tuning::set(p, definition.name, std::format("{}", definition.minimum - 1)),
                     std::out_of_range);
        EXPECT_THROW(tuning::set(p, definition.name, std::format("{}", definition.maximum + 1)),
                     std::out_of_range);
        if (definition.integer)
            EXPECT_THROW(tuning::set(p, definition.name, "3.5"), std::invalid_argument);
    }
    tuning::Parameters p;
    ASSERT_TRUE(tuning::set(p, "sEeKnIgHt", "375"));
    EXPECT_EQ(p.SeeKnight, 375);
    EXPECT_EQ(tuning::Parameters{}.SeeKnight, 350);
    EXPECT_THROW(tuning::set(p, "SeeKnight", "376"), std::out_of_range);
    EXPECT_EQ(p.SeeKnight, 375);
}

TEST(TuningTest, SeeAndQuiescenceUseActivePrices) {
    const Board position{"7k/8/5p2/4n3/3B4/8/8/K7 w - - 0 1"};
    const Move capture{D4, E5};
    search::History history;
    tuning::Parameters p;
    EXPECT_FALSE(position.see(capture, 0, p));
    EXPECT_TRUE(movegen::QuiescenceMoves(position, history, NULL_MOVE, p).next().is_null());
    p.SeeKnight = p.SeeBishop = 375;
    EXPECT_TRUE(position.see(capture, 0, p));
    EXPECT_EQ(movegen::QuiescenceMoves(position, history, NULL_MOVE, p).next(), capture);

    const Board queen{"7k/8/q7/8/8/8/7K/R7 w - - 0 1"};
    EXPECT_FALSE(queen.see(Move{A1, A6}, 1300, p));
    p.SeeQueen = 1600;
    EXPECT_TRUE(queen.see(Move{A1, A6}, 1300, p));
}

TEST(TuningTest, MainSearchSeeThresholdUsesConfiguredDivisor) {
    const Board position{"7k/8/5p2/4n3/3B4/8/8/K7 w - - 0 1"};
    search::History history;
    const auto context = history.context_for(position, 0);
    tuning::Parameters p;
    p.SeeKnight = 250;
    p.SeeBishop = 500; // Capture loses 250, exactly the threshold with divisor=1.
    const Move capture{D4, E5};
    EXPECT_NE(movegen::MainSearchMoves(position, history, context, p).next(), capture);
    p.CaptureSeeThresholdDivisor = 1;
    EXPECT_EQ(movegen::MainSearchMoves(position, history, context, p).next(), capture);
}

TEST(TuningTest, CaptureOrderingUsesWeightsHistoryAndQueenPrice) {
    const Board position{"7k/8/8/b1p1b3/Q7/3N4/6P1/7K w - - 0 1"};
    const Move knight_bishop{D3, E5}, queen_bishop{A4, A5}, knight_pawn{D3, C5};
    search::History history;
    history.update_capture(position, knight_pawn, 2560);
    tuning::Parameters p;
    auto order = [&] {
        auto moves = movegen::QuiescenceMoves(position, history, NULL_MOVE, p);
        return std::array{moves.next(), moves.next(), moves.next()};
    };
    EXPECT_EQ(order(), (std::array{knight_bishop, queen_bishop, knight_pawn}));
    p.CaptureVictimWeight = 3;
    EXPECT_EQ(order(), (std::array{knight_bishop, knight_pawn, queen_bishop}));
    p.CaptureVictimWeight = 7;
    p.CaptureHistoryDivisor = 8;
    EXPECT_EQ(order().front(), knight_pawn);
    p.CaptureHistoryDivisor = 16;
    p.SeeQueen = 1600;
    EXPECT_EQ(order(), (std::array{knight_bishop, knight_pawn, queen_bishop}));
}

TEST(TuningTest, HistoryLearningUsesConfiguredRateAndLimit) {
    const Board position{"7k/8/8/4p3/8/3N4/8/7K w - - 0 1"};
    search::History history;
    const auto context = history.context_for(position, 0);
    search::Instrumentation<> stats;
    tuning::Parameters p;
    p.CaptureLearningRate = 24;
    p.CaptureDepthSquaredLimit = 64;
    const Move capture{D3, E5};
    search::HistoryLearning learning(position, history, context, 20, stats, p);
    learning.learn_from_cutoff(capture, true);
    EXPECT_EQ(history.capture_score(position, capture), 1536);
}

class TuningEngineTest : public EngineTest {};

TEST_F(TuningEngineTest, ChangesClearSearchStateAndSurviveNewGamesAndResize) {
    auto& history = SearchTestAccess::history(SearchThreadTestAccess::worker(thread_pool()));
    const Move quiet{E2, E4};
    history.update_quiet(history.context_for(board(), 0), board(), quiet, 100);
    search::tt.store(board().tt_key(), quiet, 42, 3, search::TTBound::Exact, 0);
    ASSERT_TRUE(execute("setoption name SeeKnight value 375"));
    EXPECT_EQ(options().parameters.SeeKnight, 375);
    EXPECT_EQ(history.base_quiet_score(WHITE, quiet), 0);
    EXPECT_FALSE(search::tt.probe(board().tt_key()).record);
    ASSERT_TRUE(execute("ucinewgame"));
    ASSERT_TRUE(execute("setoption name Threads value 2"));
    ASSERT_TRUE(execute("go depth 4"));
    thread_pool().wait();
    ASSERT_TRUE(execute("setoption name SeeKnight value 250"));
    ASSERT_TRUE(execute("go depth 4"));
    thread_pool().wait();
    EXPECT_EQ(options().parameters.SeeKnight, 250);
    EXPECT_EQ(output.str().find("error:"), std::string::npos) << output.str();
}

TEST_F(TuningEngineTest, RejectsChangesDuringSearchAndInvalidValuesAtomically) {
    ASSERT_TRUE(execute("go infinite"));
    ASSERT_TRUE(execute("setoption name LmrPvScale value 0.9"));
    EXPECT_EQ(options().parameters.LmrPvScale, 0.7);
    execute("stop");
    thread_pool().wait();
    EXPECT_NE(output.str().find("cannot set option while search is in progress"),
              std::string::npos);
    execute("setoption name LmrPvScale value nan");
    EXPECT_EQ(options().parameters.LmrPvScale, 0.7);
    execute("setoption name LmrPvScale value 0.9");
    execute("ucinewgame");
    EXPECT_EQ(options().parameters.LmrPvScale, 0.9);
}

TEST(TuningTest, BoundarySearchesKeepLegalResultsAndClockBounds) {
    tuning::Parameters p;
    RecordingSearchReporter reporter;
    search::ThreadPool pool(1, reporter, p);
    const Board positions[] = {
        Board{},
        Board{"r1bq1r1k/p1pnbpp1/1p2p3/6p1/3PB3/5N2/PPPQ1PPP/2KR3R w - - 0 1"},
        Board{"8/5pk1/p4npp/1pPN4/1P2p3/1P4PP/5P2/5K2 w - - 0 1"},
    };
    search::Limits limits;
    limits.set_depth(9);
    limits.set_nodes(12000);
    auto exercise = [&] {
        for (const Board& position : positions) {
            pool.clear_search_heuristics();
            search::tt.clear();
            reporter.clear();
            ASSERT_TRUE(pool.start_search(position, limits));
            pool.wait();
            ASSERT_EQ(reporter.best_moves.size(), 1U);
            EXPECT_TRUE(position.is_legal_move(reporter.best_moves.front()));
        }
        search::Limits clock;
        clock.set_wtime(10000);
        clock.set_btime(10000);
        search::TimeManager manager(clock, WHITE, p);
        for (int depth = 1; depth < 30; ++depth) {
            manager.update_after_iteration(depth, Move{E2, E4}, depth % 2 ? 100 : -100);
            ASSERT_TRUE(manager.soft_limit());
            EXPECT_GT(manager.soft_limit()->count(), 0);
            EXPECT_LE(*manager.soft_limit(), *manager.hard_limit());
            EXPECT_LE(manager.hard_limit()->count(), 9950);
        }
    };
    for (const auto& definition : tuning::definitions()) {
        for (double boundary : {definition.minimum, definition.maximum}) {
            SCOPED_TRACE(std::format("{}={}", definition.name, boundary));
            p = {};
            ASSERT_TRUE(tuning::set(p, definition.name, std::format("{}", boundary)));
            exercise();
        }
    }
    // All-low, all-high, alternating corners, then reproducible mixed corners.
    for (unsigned pattern = 0; pattern < 8; ++pattern) {
        SCOPED_TRACE(pattern);
        unsigned index = 0;
        for (const auto& definition : tuning::definitions()) {
            bool high = pattern == 1
                || (pattern >= 2
                    && ((index++ * 2654435761U + pattern * 2246822519U) >> (pattern + 4)) % 2);
            ASSERT_TRUE(tuning::set(
                p,
                definition.name,
                std::format("{}", high ? definition.maximum : definition.minimum)));
        }
        exercise();
    }
}
#endif
