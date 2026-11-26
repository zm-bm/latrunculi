#include <gtest/gtest.h>

#include <array>
#include <format>
#include <sstream>
#include <utility>

#include "movegen/moves.hpp"
#include "search/parameters.hpp"
#include "search/tt.hpp"
#include "support/engine_test_fixture.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"
#include "tuning/options.hpp"

TEST(TuningTest, MetadataReportsDefaultsAndLimitsWithoutCampaignPolicy) {
    std::ostringstream output;
    tuning::export_parameters(output);
    EXPECT_NE(output.str().find("latrunculi.search-parameters.v1"), std::string::npos);
    EXPECT_NE(output.str().find(std::format(
                  "\"name\":\"NullMoveSurplusPerPly\",\"type\":\"int\",\"default\":{},\"min\":1",
                  search::default_parameters.null_move_surplus_per_ply)),
              std::string::npos);
    EXPECT_EQ(output.str().find("c_end"), std::string::npos);
    EXPECT_EQ(output.str().find("iterations"), std::string::npos);
    EXPECT_NE(tuning::uci_options().find(std::format(
                  "FutilityMargin1 type spin default {} min 0 max 30000",
                  search::default_parameters.futility_margin[1])),
              std::string::npos);
}

TEST(TuningTest, InvalidChangesAreAtomicAndEngineValuesStayIndependent) {
    search::Parameters a, b;
    ASSERT_TRUE(tuning::set(a, "fUtilityMargin1", "300"));
    EXPECT_EQ(a.futility_margin[1], 300);
    EXPECT_EQ(b.futility_margin[1], search::default_parameters.futility_margin[1]);
    for (const char* value :
         {"", "nan", "inf", "12 extra", "3.5", "999999999999999999", "-1", "30001"}) {
        EXPECT_THROW(tuning::set(a, "FutilityMargin1", value), std::exception);
        EXPECT_EQ(a.futility_margin[1], 300);
    }
    EXPECT_THROW(tuning::set(a, "NullMoveSurplusPerPly", "0"), std::out_of_range);
    EXPECT_EQ(a.null_move_surplus_per_ply, search::default_parameters.null_move_surplus_per_ply);
    EXPECT_FALSE(tuning::set(a, "Unknown", "12"));
}

TEST(TuningTest, WorkersKeepOneStableConfigurationAcrossResizeAndSearch) {
    search::Parameters parameters;
    parameters.qcapture_margin = 222;
    RecordingSearchReporter reporter;
    search::ThreadPool pool(2, reporter);
    ASSERT_TRUE(pool.configure_parameters(parameters));
    search::Limits limits;
    limits.set_depth(3);
    for (int count : {2, 1, 2}) {
        ASSERT_TRUE(pool.resize(count));
        for (int i = 0; i < count; ++i) {
            auto& worker = SearchThreadTestAccess::worker(pool, i);
            EXPECT_NE(&SearchTestAccess::parameters(worker), &parameters);
            EXPECT_EQ(SearchTestAccess::parameters(worker).qcapture_margin, 222);
        }
        ASSERT_TRUE(pool.start_search(Board{}, limits));
        pool.wait();
    }
}

class TuningEngineTest : public EngineTest {};

TEST_F(TuningEngineTest, UpdatesClearSearchStateAndSurviveNewGames) {
    auto& history = SearchTestAccess::history(SearchThreadTestAccess::worker(thread_pool()));
    const Move move{E2, E4};
    history.update_quiet(history.context_for(board(), 0), board(), move, 100);
    search::tt.store(board().tt_key(), move, 42, 3, search::TTBound::Exact, 0);
    ASSERT_TRUE(execute("setoption name QCaptureMargin value 222"));
    EXPECT_EQ(options().search_parameters.qcapture_margin, 222);
    EXPECT_EQ(history.base_quiet_score(WHITE, move), 0);
    EXPECT_FALSE(search::tt.probe(board().tt_key()).record);
    execute("ucinewgame");
    execute("setoption name Threads value 2");
    execute("go depth 4");
    thread_pool().wait();
    EXPECT_EQ(options().search_parameters.qcapture_margin, 222);
}

TEST_F(TuningEngineTest, ActiveSearchAndInvalidValuesCannotChangeSettings) {
    execute("go infinite");
    execute("setoption name QCaptureMargin value 222");
    EXPECT_EQ(options().search_parameters.qcapture_margin,
              search::default_parameters.qcapture_margin);
    execute("stop");
    thread_pool().wait();
    execute("setoption name NullMoveSurplusPerPly value 0");
    EXPECT_EQ(options().search_parameters.null_move_surplus_per_ply,
              search::default_parameters.null_move_surplus_per_ply);
    execute("setoption name NullMoveSurplusPerPly value 150");
    EXPECT_EQ(options().search_parameters.null_move_surplus_per_ply, 150);
}

TEST(TuningTest, CampaignBoundaryCombinationsKeepLegalSearchResults) {
    constexpr std::array names{
        "FutilityMargin1",
        "FutilityMargin2",
        "FutilityMargin3",
        "RazorMargin1",
        "RazorMargin2",
        "RazorMargin3",
        "NullMoveSurplusPerPly",
        "QCaptureMargin"};
    constexpr std::array lows{125, 200, 275, 250, 450, 900, 100, 475};
    constexpr std::array highs{375, 600, 825, 750, 1350, 2700, 300, 775};
    search::Parameters parameters;
    RecordingSearchReporter reporter;
    search::ThreadPool pool(1, reporter);
    search::Limits limits;
    limits.set_depth(5);
    limits.set_nodes(12000);
    Board position{"r1bq1r1k/p1pnbpp1/1p2p3/6p1/3PB3/5N2/PPPQ1PPP/2KR3R w - - 0 1"};
    for (unsigned pattern = 0; pattern < 4; ++pattern) {
        for (unsigned i = 0; i < names.size(); ++i)
            ASSERT_TRUE(tuning::set(
                parameters,
                names[i],
                std::to_string(
                    (pattern == 1 || (pattern >= 2 && (i + pattern) % 2)) ? highs[i] : lows[i])));
        ASSERT_TRUE(pool.configure_parameters(parameters));
        pool.clear_search_heuristics();
        search::tt.clear();
        reporter.clear();
        ASSERT_TRUE(pool.start_search(position, limits));
        pool.wait();
        ASSERT_EQ(reporter.best_moves.size(), 1U);
        EXPECT_TRUE(position.is_legal_move(reporter.best_moves.front()));
    }
}

TEST(TuningTest, FractionalOptionsAndPiecePricesAreValidatedAtomically) {
    search::Parameters p;
    ASSERT_TRUE(tuning::set(p, "LmrQuietDivisor", "2.375"));
    EXPECT_DOUBLE_EQ(p.lmr_quiet_divisor, 2.375);
    for (const char* value : {"nan", "inf", "0", "2.5 extra", "1e309"}) {
        EXPECT_THROW(tuning::set(p, "LmrQuietDivisor", value), std::exception);
        EXPECT_DOUBLE_EQ(p.lmr_quiet_divisor, 2.375);
    }
    ASSERT_TRUE(tuning::set(p, "KnightValue", "375"));
    EXPECT_THROW(tuning::set(p, "KnightValue", "376"), std::out_of_range);
    EXPECT_EQ(p.piece_values[KNIGHT], 375);
    EXPECT_TRUE(tuning::valid(p));
    EXPECT_FALSE(tuning::set(p, "SeeKnight", "360"));
    for (const auto [piece, value] :
         {std::pair{NO_PIECETYPE, 1},
          {PAWN, 101},
          {KING, 1},
          {KNIGHT, 376},
          {BISHOP, 374},
          {ROOK, 499},
          {QUEEN, 899}}) {
        auto invalid = p;
        invalid.piece_values[piece] = value;
        EXPECT_FALSE(tuning::valid(invalid));
    }
    p.piece_values[PAWN] = 101;
    EXPECT_FALSE(tuning::valid(p));
    RecordingSearchReporter reporter;
    search::ThreadPool pool(1, reporter);
    EXPECT_FALSE(pool.configure_parameters(p));
    EXPECT_EQ(SearchTestAccess::parameters(SearchThreadTestAccess::worker(pool)).piece_values[PAWN],
              100);
}

TEST(TuningTest, WorkerHistoryAndMoveOrderingSharePoolSettings) {
    search::Parameters p;
    p.piece_values[KNIGHT] = 375;
    p.capture_history_divisor = 24;
    p.capture_victim_weight = 9;
    RecordingSearchReporter reporter;
    search::ThreadPool pool(2, reporter);
    ASSERT_TRUE(pool.configure_parameters(p));
    for (int count : {2, 1, 3}) {
        ASSERT_TRUE(pool.resize(count));
        for (int i = 0; i < count; ++i) {
            auto& worker = SearchThreadTestAccess::worker(pool, i);
            const auto& values = SearchTestAccess::history(worker).parameters();
            EXPECT_EQ(&values, &SearchTestAccess::parameters(worker));
            EXPECT_EQ(values.piece_values[KNIGHT], 375);
            EXPECT_EQ(values.capture_history_divisor, 24);
            EXPECT_EQ(values.capture_victim_weight, 9);
        }
    }
}

TEST(TuningTest, MoveStagesUseTheSameTunedPiecePricesAsCaptureOrdering) {
    const Board board("7k/8/2p5/3r4/4Q3/8/8/K7 w - - 0 1");
    const Move capture(E4, D5);
    ASSERT_TRUE(board.is_legal_move(capture));
    search::History defaults;
    auto context = defaults.context_for(board, 0);
    movegen::MainSearchMoves baseline(board, defaults, context);
    EXPECT_NE(baseline.next(), capture);

    search::Parameters p;
    p.piece_values[ROOK] = 900;
    p.piece_values[QUEEN] = 900;
    ASSERT_TRUE(tuning::valid(p));
    search::History history(p);
    auto tuned_context = history.context_for(board, 0);
    movegen::MainSearchMoves tuned(board, history, tuned_context);
    EXPECT_EQ(tuned.next(), capture);
}
