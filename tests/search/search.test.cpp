#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "core/constants.hpp"
#include "eval/evaluation.hpp"
#include "movegen/moves.hpp"
#include "search/thread_pool.hpp"
#include "search/tt.hpp"
#include "search/worker.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_reporter.hpp"
#include "support/search_test_access.hpp"
#include "support/search_thread_test_access.hpp"

namespace search {

namespace {

class SearchTest : public ::testing::Test {
protected:
    RecordingSearchReporter reporter;
    ThreadPool              pool{1, reporter};
    Thread&                 thread{SearchThreadTestAccess::thread(pool)};
    Worker&                 worker{SearchThreadTestAccess::worker(thread)};
    Limits                  limits;
    int                     current_ply{0};

    void SetUp() override {
        limits.depth = 4;
        tt.clear();
    }

    void load(const Board& board, int depth = 4) {
        tt.clear();
        history().clear();
        limits.depth = depth;
        worker.configure_search(board, limits, SearchClock::now());
        SearchTestAccess::reset(worker);
        current_ply = 0;
    }

    Board&   position() { return SearchTestAccess::board(worker); }
    int&     ply() { return current_ply; }
    History& history() { return SearchTestAccess::history(worker); }

    Score search(Score alpha, Score beta, int depth) {
        return SearchTestAccess::alphabeta<NodeType::NonPv>(
            worker, alpha, beta, depth, nullptr, ply());
    }

    void load_after_null(const Board& parent, int depth) {
        Board board = parent;
        board.make_null();
        load(board, depth);
        ply() = 1;
        ASSERT_TRUE(position().previous_move().is_null());
        ASSERT_TRUE(position().can_unmake());
    }

    Score pv_search(Score alpha, Score beta, int depth, PV& pv) {
        return SearchTestAccess::alphabeta<NodeType::Pv>(worker, alpha, beta, depth, &pv, ply());
    }

    Move find_move(std::string_view move_string) {
        return movegen::UnscoredMoves(position()).next_matching([&](Move move) {
            return move.str() == move_string;
        });
    }

    template <typename Fn>
    auto with_move(Move move, Fn&& fn) {
        using Result = std::invoke_result_t<Fn&>;
        position().make(move);
        ++ply();
        if constexpr (std::is_void_v<Result>) {
            fn();
            position().unmake();
            --ply();
        } else {
            Result result = fn();
            position().unmake();
            --ply();
            return result;
        }
    }

    template <typename Fn>
    auto with_null_move(Fn&& fn) {
        using Result = std::invoke_result_t<Fn&>;
        position().make_null();
        ++ply();
        if constexpr (std::is_void_v<Result>) {
            fn();
            position().unmake_null();
            --ply();
        } else {
            Result result = fn();
            position().unmake_null();
            --ply();
            return result;
        }
    }

    std::vector<Move> ordered_moves(Move tt_move = NULL_MOVE) {
        std::vector<Move> moves;
        const auto        context = history().context_for(position(), ply(), tt_move);
        auto              picker  = movegen::MainSearchMoves(position(), history(), context);
        for (Move move = picker.next(); !move.is_null(); move = picker.next())
            moves.push_back(move);
        return moves;
    }

    void store_child(Move move, Score score, int depth, TTBound bound = TTBound::Exact) {
        with_move(move,
                  [&] { tt.store(position().tt_key(), NULL_MOVE, score, depth, bound, ply()); });
    }

    PositionKey null_child_key() {
        Board copy{position().to_fen()};
        copy.make_null();
        return copy.tt_key();
    }

    PositionKey descendant_null_key(Move move) {
        Board copy{position().to_fen()};
        copy.make(move);
        copy.make_null();
        return copy.tt_key();
    }

    std::optional<TTRecord> record() const {
        return tt.probe(SearchTestAccess::board(worker).tt_key()).record;
    }

    int quiet_history(Move move) {
        return SearchTestAccess::quiet_history_entry(
            history(), position().side_to_move(), move.from(), move.to());
    }

    int continuation_history(Color prev_c, PieceType prev_piece, Square prev_to, Move move) {
        return SearchTestAccess::continuation_history_entry(history(),
                                                            prev_c,
                                                            prev_piece,
                                                            prev_to,
                                                            position().piece_type_on(move.from()),
                                                            move.to());
    }

#if LATRUNCULI_SEARCH_STATS
    const Counters& counters() { return SearchTestAccess::instrumentation(worker).raw_counters(); }
#endif
};

} // namespace

TEST_F(SearchTest, HandlesDrawAndMaxPlyExits) {
    Board drawn{board_test::fen::kings_only};
    load(drawn);
    EXPECT_EQ(search(-scores::inf, scores::inf, 2), scores::draw);

    Board max_ply{board_test::fen::quiet_black_to_move};
    load(max_ply);
    ply() = engine::max_search_ply;
    auto& terminal = SearchTestAccess::frames(worker)[ply()];
    terminal.pv.update(Move{E2, E4}, PV{});
    PV pv;
    EXPECT_EQ(pv_search(-scores::inf, scores::inf, 1, pv), eval::evaluate(position()));
    EXPECT_TRUE(terminal.pv_enabled);
    EXPECT_TRUE(terminal.pv.empty());
    EXPECT_TRUE(pv.empty());
    EXPECT_FALSE(record());

    Board drawn_max{"k7/8/8/8/8/8/4r3/K2Q4 w - - 100 1"};
    load(drawn_max);
    ply() = engine::max_search_ply;
    EXPECT_EQ(search(-scores::inf, scores::inf, 1), scores::draw);
}

TEST_F(SearchTest, AdjudicatesFiftyMoveBoundaryBeforeTtAndMaxPly) {
    struct Case {
        const char* fen;
        int         search_ply;
        Score       value;
    };
    constexpr std::array cases{
        Case{"7k/6Q1/5K2/8/8/8/8/8 b - - 100 1", 3, -scores::mate + 3},
        Case{"7k/6Q1/5K2/8/8/8/8/8 b - - 100 1",
             engine::max_search_ply,
             -scores::mate + engine::max_search_ply},
        Case{"7k/7Q/5K2/8/8/8/8/8 b - - 100 1", 3, scores::draw},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.fen);
        SCOPED_TRACE(tc.search_ply);
        Board board{tc.fen};
        load(board);
        ply() = tc.search_ply;
        tt.store(position().tt_key(), NULL_MOVE, 1234, 8, TTBound::Exact, ply());
        EXPECT_EQ(search(-1, 0, 2), tc.value);
        PV pv;
        EXPECT_EQ(pv_search(-scores::inf, scores::inf, 2, pv), tc.value);
    }
}

TEST_F(SearchTest, ReturnsFailSoftValues) {
    constexpr auto one_evasion = board_test::fen::one_legal_evasion;
    constexpr int  depth       = 1;
    const Move     evasion{A8, B8};

    struct Case {
        const char* name;
        Score       alpha;
        Score       beta;
    };

    constexpr std::array cases{
        Case{"fail low", -100, 0},
        Case{"fail high", -1000, -900},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board child_board{one_evasion};
        load(child_board, depth);
        ASSERT_TRUE(position().is_legal_move(evasion));
        const Score expected =
            -with_move(evasion, [&] { return search(-tc.beta, -tc.alpha, depth - 1); });

        Board board{one_evasion};
        load(board, depth);
        const Score actual = search(tc.alpha, tc.beta, depth);
        EXPECT_EQ(actual, expected);
        EXPECT_NE(actual, tc.name == std::string_view{"fail low"} ? tc.alpha : tc.beta);
    }
}

TEST_F(SearchTest, AppliesMateDistanceBounds) {
    Board board{board_test::fen::quiet_black_to_move};
    load(board);
    ply() = 4;

    constexpr Score lower = -scores::mate + 4;
    EXPECT_EQ(search(-scores::inf, lower - 1, 1), lower);

    load(board);
    ply()                 = 4;
    constexpr Score upper = scores::mate - 5;
    EXPECT_EQ(search(upper + 1, scores::inf, 1), upper + 1);
}

TEST_F(SearchTest, UsesOnlyDepthEligibleTtBounds) {
    struct Case {
        const char* name;
        TTBound     bound;
        Score       score;
        Score       alpha;
        Score       beta;
    };

    constexpr std::array cases{
        Case{"exact", TTBound::Exact, 321, -scores::inf, scores::inf},
        Case{"lower", TTBound::LowerBound, 500, 100, 200},
        Case{"upper", TTBound::UpperBound, -500, -200, -100},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{board_test::fen::quiet_black_to_move};
        load(board, 2);
        tt.store(position().tt_key(), NULL_MOVE, tc.score, 2, tc.bound, ply());
        EXPECT_EQ(search(tc.alpha, tc.beta, 2), tc.score);

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().main_tt_cutoffs[0], 1U);
        EXPECT_EQ(counters().q_tt_probes[0], 0U);
#endif
    }

    Board baseline_board{board_test::fen::quiet_black_to_move};
    load(baseline_board, 2);
    const Score baseline = search(-scores::inf, scores::inf, 2);

    Board shallow_board{board_test::fen::quiet_black_to_move};
    load(shallow_board, 2);
    tt.store(position().tt_key(), Move(H1, H2), baseline + 500, 1, TTBound::Exact, ply());
    EXPECT_EQ(search(-scores::inf, scores::inf, 2), baseline);
}

TEST_F(SearchTest, StoresWindowClassifiedTtBounds) {
    Board exact_board{board_test::fen::quiet_black_to_move};
    load(exact_board, 2);
    const Score exact  = search(-scores::inf, scores::inf, 2);
    auto        stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->score_at_ply(ply()), exact);
    EXPECT_EQ(stored->depth, 2);
    EXPECT_EQ(stored->bound, TTBound::Exact);

    Board lower_source{board_test::fen::one_legal_evasion};
    load(lower_source, 1);
    const Score full = search(-scores::inf, scores::inf, 1);

    Board lower_board{board_test::fen::one_legal_evasion};
    load(lower_board, 1);
    EXPECT_GE(search(full - 150, full - 50, 1), full - 50);
    stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->bound, TTBound::LowerBound);

    Board upper_board{board_test::fen::one_legal_evasion};
    load(upper_board, 1);
    EXPECT_LT(search(-100, 0, 1), -100);
    stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->bound, TTBound::UpperBound);

    Board mate{"7k/6Q1/6K1/8/8/8/8/8 b - - 0 1"};
    load(mate, 2);
    ply() = 3;
    EXPECT_EQ(search(-scores::inf, scores::inf, 2), -scores::mate + 3);
    stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->score, -scores::mate);
    EXPECT_EQ(stored->bound, TTBound::Exact);
    EXPECT_EQ(stored->move, NULL_MOVE);
}

TEST_F(SearchTest, SingularExclusionDoesNotCreateMateOrStalemate) {
    for (const char* fen : {"k7/8/1PK5/8/8/8/P7/8 b - - 0 1", board_test::fen::one_legal_evasion}) {
        SCOPED_TRACE(fen);
        load(Board{fen});
        ply()           = 1;
        const Move only = find_move("a8b8");
        ASSERT_FALSE(only.is_null());
        tt.store(position().tt_key(), only, 1000, 9, TTBound::Exact, ply());
        SearchTestAccess::frames(worker)[ply()].excluded_move = only;
        SearchTestAccess::frames(worker)[ply()].singular_search_active = true;

        EXPECT_EQ(search(200, 201, 3), 200);
        EXPECT_EQ(position().to_fen(), fen);
        const auto stored = record();
        ASSERT_TRUE(stored);
        EXPECT_EQ(stored->score_at_ply(ply()), 1000);
        EXPECT_EQ(stored->depth, 9);
        EXPECT_EQ(stored->bound, TTBound::Exact);
        EXPECT_EQ(stored->move, only);
    }
}

TEST_F(SearchTest, SingularExtensionRequiresFailLow) {
    // At depth eight, a TT score of 1000 gives a singular beta of 984.
    for (Score alternative_score : {983, 984}) {
        SCOPED_TRACE(alternative_score);
        load(Board{board_test::fen::quiet_black_to_move}, 12);
        ply()                                          = 1;
        SearchTestAccess::root_iteration_depth(worker) = 12;
        const Move tt_move                             = find_move("a8b8");
        store_child(find_move("a8a7"), -alternative_score, engine::max_search_depth);
        tt.store(position().tt_key(), tt_move, 1000, 5, TTBound::LowerBound, ply());

        search(999, 1000, 8);
        EXPECT_EQ(position().to_fen(), board_test::fen::quiet_black_to_move);
        EXPECT_TRUE(SearchTestAccess::frames(worker)[ply()].excluded_move.is_null());
        EXPECT_FALSE(SearchTestAccess::frames(worker)[ply()].singular_search_active);
        const auto child = with_move(tt_move, [&] { return record(); });
        ASSERT_TRUE(child);
        EXPECT_EQ(child->depth, alternative_score < 984 ? 8 : 7);
    }
}

TEST_F(SearchTest, SingularExtensionBuildsLegalPv) {
    load(Board{board_test::fen::one_legal_evasion}, 12);
    ply()                                          = 1;
    SearchTestAccess::root_iteration_depth(worker) = 12;
    const Move only                                = find_move("a8b8");
    tt.store(position().tt_key(), only, 30000, 7, TTBound::LowerBound, ply());

    PV pv;
    pv_search(-scores::inf, scores::inf, 8, pv);
    ASSERT_FALSE(pv.empty());
    EXPECT_EQ(pv.front(), only);
    const auto child = with_move(only, [&] { return record(); });
    ASSERT_TRUE(child);
    EXPECT_GE(child->depth, 8);

    Board replay{position()};
    for (int index = 0; index < pv.size(); ++index) {
        const Move move = pv.move_at(index);
        ASSERT_TRUE(replay.is_legal_move(move));
        replay.make(move);
    }
}

TEST_F(SearchTest, SingularMulticutLeavesTtAndHistoryUnchanged) {
    load(Board{board_test::fen::quiet_black_to_move}, 12);
    ply()                                          = 1;
    SearchTestAccess::root_iteration_depth(worker) = 12;
    const Move tt_move                             = find_move("a8b8");
    const Move alternative                         = find_move("a8a7");
    store_child(alternative, -1134, engine::max_search_depth);
    tt.store(position().tt_key(), tt_move, 1000, 5, TTBound::LowerBound, ply());

    // Return singular beta (1000 - 16), not the alternative's fail-soft score of 1134.
    EXPECT_EQ(search(983, 984, 8), 984);
    EXPECT_EQ(position().to_fen(), board_test::fen::quiet_black_to_move);
    const auto stored = record();
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->depth, 5);
    EXPECT_EQ(stored->score_at_ply(ply()), 1000);
    EXPECT_EQ(stored->bound, TTBound::LowerBound);
    EXPECT_EQ(stored->move, tt_move);
    for (Move move : {tt_move, alternative}) {
        EXPECT_EQ(quiet_history(move), 0);
        EXPECT_FALSE(history().is_killer(move, ply()));
    }
    auto& frame = SearchTestAccess::frames(worker)[ply()];
    EXPECT_EQ(frame.static_eval, eval::evaluate(position()));
    EXPECT_TRUE(frame.excluded_move.is_null());
    EXPECT_FALSE(frame.singular_search_active);

    // Inside a verification subtree, this same result must not take a multicut.
    frame.singular_search_active = true;
    (void)search(983, 984, 8);
    ASSERT_TRUE(record());
    EXPECT_EQ(record()->depth, 8);
    EXPECT_TRUE(frame.singular_search_active);
    EXPECT_TRUE(frame.excluded_move.is_null());
    const auto& child = SearchTestAccess::frames(worker)[ply() + 1];
    EXPECT_TRUE(child.singular_search_active);
    EXPECT_TRUE(child.excluded_move.is_null());

    // A later ordinary call reuses that child frame without the inherited restriction.
    frame.singular_search_active = false;
    PV pv;
    (void)pv_search(-scores::inf, scores::inf, 1, pv);
    EXPECT_FALSE(child.singular_search_active);
    EXPECT_FALSE(frame.singular_search_active);
}

TEST_F(SearchTest, InterruptedSingularSearchRestoresState) {
    for (bool was_active : {false, true}) {
        SCOPED_TRACE(was_active);
        limits.set_nodes(4096);
        load(Board{board_test::fen::quiet_black_to_move}, 12);
        ply()                                          = 1;
        SearchTestAccess::root_iteration_depth(worker) = 12;
        tt.store(position().tt_key(), find_move("a8b8"), 1000, 5, TTBound::LowerBound, ply());
        SearchTestAccess::frames(worker)[ply()].singular_search_active = was_active;
        auto& frame                                      = SearchTestAccess::frames(worker)[ply()];
        frame.static_eval                                = 456;
        frame.real_move                                  = true;

        // The parent and excluded root run; the next child polls the node limit.
        SearchTestAccess::set_nodes(worker, 4094);
        PV pv;
        EXPECT_EQ(pv_search(-1000, 1000, 8, pv), -1000);
        EXPECT_EQ(worker.node_count(), 4096U);
        EXPECT_EQ(position().to_fen(), board_test::fen::quiet_black_to_move);
        EXPECT_FALSE(frame.static_eval);
        EXPECT_FALSE(frame.real_move);
        EXPECT_TRUE(frame.excluded_move.is_null());
        EXPECT_EQ(SearchTestAccess::frames(worker)[ply()].singular_search_active, was_active);
        EXPECT_TRUE(pv.empty());
        EXPECT_TRUE(frame.pv_enabled);
        EXPECT_TRUE(frame.pv.empty());
        const auto stored = record();
        ASSERT_TRUE(stored);
        EXPECT_EQ(stored->depth, 5);
    }
}

TEST_F(SearchTest, NullMovePruningReturnsFailSoftCutoff) {
    Board board{board_test::fen::start};
    load(board, 4);
    const PositionKey root_key = position().tt_key();
    tt.store(null_child_key(), NULL_MOVE, -200, 1, TTBound::Exact, 1);

    auto& frames                     = SearchTestAccess::frames(worker);
    frames[0].singular_search_active = true;
    EXPECT_EQ(search(-50, 50, 4), 200);
    EXPECT_TRUE(frames[1].singular_search_active);
    EXPECT_TRUE(frames[0].singular_search_active);
    EXPECT_FALSE(tt.probe(root_key).record.has_value());

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().null_move_tries[0], 1U);
    EXPECT_EQ(counters().null_move_cutoffs[0], 1U);
#endif
}

TEST_F(SearchTest, PvNodesDoNotUseNullMovePruning) {
    Board baseline_board{board_test::fen::start};
    load(baseline_board, 4);
    PV          baseline_pv;
    const Score baseline = pv_search(-50, 50, 4, baseline_pv);

    Board board{board_test::fen::start};
    load(board, 4);
    tt.store(null_child_key(), NULL_MOVE, -200, 1, TTBound::Exact, 1);
    PV pv;
    EXPECT_EQ(pv_search(-50, 50, 4, pv), baseline);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().null_move_tries[0], 0U);
#endif
}

TEST_F(SearchTest, NullMovePruningRequiresAllGuards) {
    struct Case {
        const char* name;
        const char* fen;
        int         depth;
        bool        after_null;
    };

    constexpr std::array cases{
        Case{"in check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", 4, false},
        Case{"no non-pawn material", board_test::fen::quiet_black_to_move, 4, false},
        Case{"lone rook", "4k3/8/8/8/8/8/8/4K2R w - - 0 1", 4, false},
        Case{"shallow", board_test::fen::start, 2, false},
        Case{"repeated null", board_test::fen::start, 4, true},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board baseline_board{tc.fen};
        if (tc.after_null)
            load_after_null(baseline_board, tc.depth);
        else
            load(baseline_board, tc.depth);
        const Score baseline = search(-50, 50, tc.depth);

        Board board{tc.fen};
        if (tc.after_null)
            load_after_null(board, tc.depth);
        else
            load(board, tc.depth);
        tt.store(null_child_key(),
                 NULL_MOVE,
                 -200,
                 std::max(0, tc.depth - 3),
                 TTBound::Exact,
                 ply() + 1);
        EXPECT_EQ(search(-50, 50, tc.depth), baseline);

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().null_move_tries[ply()], 0U);
#endif
    }
}

TEST_F(SearchTest, NullMovePruningHonorsParentUpperBoundVeto) {
    Board board{board_test::fen::start};
    load(board, 4);
    tt.store(position().tt_key(), NULL_MOVE, 49, 4, TTBound::UpperBound, ply());
    tt.store(null_child_key(), NULL_MOVE, -200, 1, TTBound::Exact, 1);
    EXPECT_NE(search(-50, 50, 4), 200);
    const auto stored = record();
    ASSERT_TRUE(stored);
    EXPECT_FALSE(stored->move.is_null());

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().null_move_tries[0], 0U);
#endif
}

TEST_F(SearchTest, NullMoveReenablesAfterARealDescendantMove) {
    load_after_null(Board{board_test::fen::start}, 5);
    const Move real_move = find_move("e7e5");
    ASSERT_FALSE(real_move.is_null());
    const PositionKey immediate  = null_child_key();
    const PositionKey descendant = descendant_null_key(real_move);

    tt.store(position().tt_key(), real_move, 0, 0, TTBound::LowerBound, ply());
    (void)search(-50, 50, 5);

    EXPECT_FALSE(tt.probe(immediate).record.has_value());
    EXPECT_TRUE(tt.probe(descendant).record.has_value());
}

TEST_F(SearchTest, NullMoveContextRequiresAnActualNullSearchEdge) {
    load(Board{board_test::fen::start}, 1);
    ply() = 1;
    ASSERT_FALSE(position().can_unmake());
    const Score static_eval = eval::evaluate(position());
    const Score beta        = static_eval - 250;

    // A loaded position at a nonzero ply still permits reverse futility.
    EXPECT_EQ(search(beta - 1, beta, 1), static_eval);
    EXPECT_EQ(worker.node_count(), 1U);
    EXPECT_FALSE(record());
}

TEST_F(SearchTest, AfterNullMoveSkipsReverseFutilityButRetainsOrdinaryTtStores) {
    load_after_null(Board{board_test::fen::start}, 1);
    const Score static_eval = eval::evaluate(position());
    const Score beta        = static_eval - 250;
    const Score expected    = static_eval + 200;
    const Move  first       = ordered_moves().front();
    store_child(first, -expected, 0);
    EXPECT_EQ(search(beta - 1, beta, 1), expected);
    const auto stored = record();
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->bound, TTBound::LowerBound);
    EXPECT_EQ(stored->depth, 1);
    EXPECT_EQ(stored->score_at_ply(ply()), expected);
}

TEST_F(SearchTest, AfterNullMoveStillAllowsMoveLoopFutility) {
    load_after_null(Board{board_test::fen::start}, 2);
    const Score alpha = eval::evaluate(position()) + 401;
    const auto  moves = ordered_moves();
    ASSERT_GT(moves.size(), 1U);
    (void)search(alpha, alpha + 100, 2);
    const auto stored = record();
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->depth, 2);
    EXPECT_GE(worker.node_count(), 2U);
    with_move(moves[1], [&] { EXPECT_FALSE(record()); });
#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().futility_skips[ply()], 1U);
#endif
}

TEST_F(SearchTest, RazoringRestoresOutputAndContinuesUnlessFailLow) {
    Board board{board_test::fen::quiet_black_to_move};
    load(board, 2);
    const Score static_eval = eval::evaluate(position());
    PV          pv;
    EXPECT_EQ(SearchTestAccess::alphabeta<NodeType::NonPv>(
                  worker, static_eval + 901, static_eval + 1001, 2, &pv),
              static_eval);
    EXPECT_TRUE(SearchTestAccess::frames(worker)[0].pv_enabled);
    EXPECT_TRUE(pv.empty());
    const auto stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->depth, 0);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().razor_cutoffs[0], 1U);
#endif

    // The queen capture raises the razor result above alpha, so main search continues.
    load(Board{"4k3/8/8/8/2q5/3P4/8/4K3 w - - 0 1"}, 1);
    const Score alpha = eval::evaluate(position()) + 500;
    auto&       frame = SearchTestAccess::frames(worker)[0];
    frame.pv.update(Move{E2, E4}, PV{});
    SearchTestAccess::frames(worker)[1].pv.update(Move{E7, E5}, PV{});
    EXPECT_GE(SearchTestAccess::alphabeta<NodeType::NonPv>(worker, alpha, alpha + 1, 1, &pv),
              alpha + 1);
    EXPECT_TRUE(frame.pv_enabled);
    EXPECT_EQ(frame.static_eval, eval::evaluate(position()));
    ASSERT_EQ(pv.size(), 1);
    EXPECT_EQ(pv.front(), Move(D3, C4));
    ASSERT_TRUE(record());
    EXPECT_EQ(record()->depth, 1);
#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().razor_tries[0], 1U);
    EXPECT_EQ(counters().razor_cutoffs[0], 0U);
#endif
}

TEST_F(SearchTest, RazoringRequiresAllGuards) {
    struct Case {
        const char* name;
        const char* fen;
        int         depth;
        bool        after_null;
        bool        pv;
        bool        seed_tt;
        int         alpha_offset;
    };

    constexpr std::array cases{
        Case{"PV", board_test::fen::quiet_black_to_move, 2, false, true, false, 901},
        Case{"check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", 2, false, false, false, 901},
        Case{"deep", board_test::fen::quiet_black_to_move, 4, false, false, false, 1901},
        Case{"after null", board_test::fen::start, 2, true, false, false, 901},
        Case{"TT move", board_test::fen::start, 2, false, false, true, 901},
        Case{"inside margin", board_test::fen::start, 2, false, false, false, 899},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{tc.fen};
        if (tc.after_null)
            load_after_null(board, tc.depth);
        else
            load(board, tc.depth);
        if (tc.seed_tt) {
            const Move move = ordered_moves().front();
            tt.store(position().tt_key(), move, 0, 0, TTBound::Exact, ply());
        }
        const Score alpha = eval::evaluate(position()) + tc.alpha_offset;
        if (tc.pv) {
            PV pv;
            (void)pv_search(alpha, alpha + 100, tc.depth, pv);
        } else {
            (void)search(alpha, alpha + 100, tc.depth);
        }
        ASSERT_TRUE(record().has_value());
        EXPECT_EQ(record()->depth, tc.depth);
    }
}

TEST_F(SearchTest, ReverseFutilityPruningReturnsStaticEval) {
    Board board{board_test::fen::start};
    load(board, 1);
    const Score static_eval = eval::evaluate(position());
    const Score beta        = static_eval - 250;

    EXPECT_EQ(search(beta - 1, beta, 1), static_eval);
    EXPECT_FALSE(record().has_value());
}

TEST_F(SearchTest, FutilitySkipsOnlyAfterFirstLegalQuiet) {
    Board expected_board{board_test::fen::start};
    load(expected_board, 2);
    const Score alpha    = eval::evaluate(position()) + 401;
    const Score beta     = alpha + 100;
    const Move  first    = ordered_moves().front();
    const Score expected = with_move(first, [&] { return -search(-beta, -alpha, 1); });

    Board board{board_test::fen::start};
    load(board, 2);
    EXPECT_EQ(search(alpha, beta, 2), expected);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().futility_skips[0], 1U);
#endif
}

TEST_F(SearchTest, FutilityRequiresAllGuards) {
    Board       eval_board{board_test::fen::start};
    const Score static_eval = eval::evaluate(eval_board);
    struct Case {
        const char* name;
        const char* fen;
        int         depth;
        Score       alpha;
        bool        pv;
    };
    const std::array cases{
        Case{"PV", board_test::fen::start, 2, static_eval + 401, true},
        Case{"check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", 2, 1000, false},
        Case{"deep", board_test::fen::start, 4, static_eval + 401, false},
        Case{"mate alpha", board_test::fen::start, 2, scores::mate_bound, false},
        Case{"inside margin", board_test::fen::start, 2, static_eval + 399, false},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{tc.fen};
        load(board, tc.depth);
        if (tc.alpha >= scores::mate_bound) {
            // Bypass razoring so this case reaches the move-loop futility guard.
            tt.store(
                position().tt_key(), ordered_moves().front(), 0, 0, TTBound::UpperBound, ply());
        }
        if (tc.pv) {
            PV pv;
            (void)pv_search(tc.alpha, tc.alpha + 100, tc.depth, pv);
        } else {
            (void)search(tc.alpha, tc.alpha + 100, tc.depth);
        }
#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().futility_skips[0], 0U);
#endif
    }
}

TEST_F(SearchTest, FutilityKeepsTacticalMoves) {
    struct Case {
        const char* fen;
        Move        first;
        Move        tactical;
        bool        killer;
    };
    constexpr std::array cases{
        Case{"4k3/8/8/8/8/8/R6r/4K3 w - - 0 1", Move(E1, D1), Move(A2, H2), false},
        Case{
            "4k3/P6p/8/8/8/8/8/4K3 w - - 0 1", Move(E1, D1), Move(A7, A8, MOVE_PROM, QUEEN), false},
        Case{"4k3/8/8/8/8/8/R7/4K3 w - - 0 1", Move(A2, A3), Move(A2, E2), true},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.fen);
        Board board{tc.fen};
        load(board, 2);
        const Score alpha = eval::evaluate(position()) + 401;
        const Score beta  = alpha + 1000;
        tt.store(position().tt_key(), tc.first, 0, 0, TTBound::Exact, ply());
        if (tc.killer)
            history().update_quiet_refutations(OrderingContext{.ply = ply()}, tc.tactical);
        store_child(tc.tactical, -(beta + 100), 1);
        EXPECT_GE(search(alpha, beta, 2), beta);
    }
}

TEST_F(SearchTest, FutilityKeepsLaterCheckingQuietHintsAndGeneratedMoves) {
    for (const bool checking_is_hint : {true, false}) {
        SCOPED_TRACE(checking_is_hint ? "quiet hint" : "generated quiet");
        Board board{"4k3/8/8/8/8/8/R7/4K3 w - - 0 1"};
        load(board, 2);

        const Move first{A2, A3};
        const Move skipped{E1, D1};
        const Move checking{A2, E2};
        ASSERT_TRUE(position().is_legal_move(first));
        ASSERT_TRUE(position().is_legal_move(skipped));
        ASSERT_TRUE(position().is_legal_move(checking));
        EXPECT_FALSE(position().gives_check(skipped));
        EXPECT_TRUE(position().gives_check(checking));

        if (checking_is_hint)
            history().update_quiet_refutations(OrderingContext{.ply = ply()}, checking);
        history().update_quiet_refutations(OrderingContext{.ply = ply()}, skipped);

        const auto moves       = ordered_moves(first);
        const auto skipped_it  = std::find(moves.begin(), moves.end(), skipped);
        const auto checking_it = std::find(moves.begin(), moves.end(), checking);
        ASSERT_FALSE(moves.empty());
        ASSERT_EQ(moves.front(), first);
        ASSERT_NE(skipped_it, moves.end());
        ASSERT_NE(checking_it, moves.end());
        ASSERT_LT(skipped_it, checking_it);

        const Score alpha = eval::evaluate(position()) + 401;
        const Score beta  = alpha + 1000;
        tt.store(position().tt_key(), first, 0, 0, TTBound::Exact, ply());
        store_child(checking, -(beta + 100), 1);
        EXPECT_GE(search(alpha, beta, 2), beta);

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().futility_skips[0], 1U);
#endif
    }
}

TEST_F(SearchTest, LateMovePruningRequiresNonPvAndNegativeCombinedHistory) {
    const auto run_case = [&](bool negative_history, bool pv_node) {
        Board board{board_test::fen::start};
        load(board, 2);
        const Move previous = find_move("e2e4");
        Score      result   = -scores::inf;

        with_move(previous, [&] {
            auto moves = ordered_moves();
            ASSERT_GT(moves.size(), 12U);
            const Move late_quiet = moves.back();
            ASSERT_EQ(late_quiet.type(), BASIC_MOVE);
            ASSERT_FALSE(position().is_capture(late_quiet));
            ASSERT_FALSE(position().gives_check(late_quiet));

            if (negative_history) {
                SearchTestAccess::quiet_history_entry(
                    history(), position().side_to_move(), late_quiet.from(), late_quiet.to()) = 4;
                SearchTestAccess::continuation_history_entry(
                    history(),
                    WHITE,
                    PAWN,
                    E4,
                    position().piece_type_on(late_quiet.from()),
                    late_quiet.to()) = -9;
            }

            moves              = ordered_moves();
            const auto late_it = std::find(moves.begin(), moves.end(), late_quiet);
            ASSERT_NE(late_it, moves.end());
            ASSERT_GT(late_it - moves.begin(), 12);
            EXPECT_EQ(history().quiet_score(
                          history().context_for(position(), ply()), position(), late_quiet)
                          < 0,
                      negative_history);

            for (Move move : moves)
                store_child(move, move == late_quiet ? -200 : 0, 1);

            if (pv_node) {
                PV pv;
                result = pv_search(-200, 100, 2, pv);
                // Searching the late child replaces its seeded null TT move.
                with_move(late_quiet, [&] {
                    const auto searched = record();
                    ASSERT_TRUE(searched.has_value());
                    EXPECT_FALSE(searched->move.is_null());
                });
            } else {
                result = search(-200, 100, 2);
            }
        });

        return result;
    };

    EXPECT_EQ(run_case(false, false), 200);
    EXPECT_EQ(run_case(true, false), 0);
    run_case(true, true);
}

TEST_F(SearchTest, QuietCutoffWithoutRealPreviousMoveSkipsContextUpdates) {
    const auto run_case = [&](bool after_null) {
        Board board{board_test::fen::start};
        load(board, 2);
        const auto test = [&] {
            const Move cutoff = ordered_moves().front();
            store_child(cutoff, -200, 1);
            EXPECT_EQ(search(-200, 100, 2), 200);
            EXPECT_EQ(SearchTestAccess::counter_move_entry(history(), WHITE, PAWN, E4), NULL_MOVE);
            EXPECT_EQ(continuation_history(WHITE, PAWN, E4, cutoff), 0);
        };
        if (after_null)
            with_null_move(test);
        else
            test();
    };

    run_case(false);
    run_case(true);
}

TEST_F(SearchTest, CaptureCutoffSkipsShallowRewardsAndPenalizesEarlierNonCutoffs) {
    struct Case {
        int depth;
        int penalty;
        int reward;
    };
    for (const auto& tc : {Case{1, -16, 0}, Case{2, -64, 0}, Case{3, -144, 144}}) {
        SCOPED_TRACE(tc.depth);
        Board board{"7k/8/8/2p1p3/5p2/3N4/6P1/4K3 b - - 0 1"};
        board.make(Move(H8, G8));
        load(board, tc.depth);
        const auto moves = ordered_moves();
        ASSERT_GE(moves.size(), 3U);
        ASSERT_TRUE(position().is_capture(moves[0]));
        ASSERT_TRUE(position().is_capture(moves[1]));
        tt.store(position().tt_key(), NULL_MOVE, 0, 0, TTBound::UpperBound, ply(), 0);
        store_child(moves[0], 0, tc.depth - 1);
        store_child(moves[1], -200, tc.depth - 1);
        EXPECT_EQ(search(-200, 100, tc.depth), 200);
        EXPECT_EQ(history().capture_score(position(), moves[0]), tc.penalty);
        EXPECT_EQ(history().capture_score(position(), moves[1]), tc.reward);
        EXPECT_EQ(history().capture_score(position(), Move(D3, F4)), 0);
        EXPECT_EQ(history().context_for(position(), ply()).primary_killer, NULL_MOVE);
        EXPECT_EQ(history().context_for(position(), ply()).counter_move, NULL_MOVE);
    }
}

TEST_F(SearchTest, CaptureHistorySkipsPromotionsEvasionsAndQuiescenceUpdates) {
    const auto cutoff = [&](const Board& board, Move move) {
        load(board, 3);
        tt.store(position().tt_key(), move, 0, 0, TTBound::UpperBound, ply(), 0);
        store_child(move, -200, 2);
        EXPECT_EQ(search(-200, 100, 3), 200);
        EXPECT_EQ(history().context_for(position(), ply()).primary_killer, NULL_MOVE);
        EXPECT_EQ(history().context_for(position(), ply()).counter_move, NULL_MOVE);
    };
    cutoff(Board{"4k2r/6P1/8/8/8/8/8/K7 w - - 0 1"}, Move(G7, H8, MOVE_PROM, QUEEN));
    EXPECT_EQ(SearchTestAccess::capture_history_entry(history(), WHITE, PAWN, H8, ROOK), 0);
    cutoff(Board{"1r5k/8/8/8/1b6/P7/2N5/1Q2K3 w - - 0 1"}, Move(A3, B4));
    EXPECT_EQ(SearchTestAccess::capture_history_entry(history(), WHITE, PAWN, B4, BISHOP), 0);
    Board quiet_promotion{"4k3/8/8/8/8/8/p7/4K3 w - - 0 1"};
    quiet_promotion.make(Move(E1, D1));
    cutoff(quiet_promotion, Move(A2, A1, MOVE_PROM, QUEEN));
    load(Board{"7k/8/8/2p1p3/8/3N4/6P1/4K3 w - - 0 1"}, 1);
    const Move capture = ordered_moves().front();
    tt.store(position().tt_key(), NULL_MOVE, 0, 0, TTBound::UpperBound, ply(), 0);
    store_child(capture, -200, 0);
    EXPECT_EQ(SearchTestAccess::quiescence<NodeType::NonPv>(worker, -200, 100), 200);
    EXPECT_EQ(history().capture_score(position(), capture), 0);
}

TEST_F(SearchTest, QuietCutoffTrainsHistoriesAndExemptsTtAndKillerHints) {
    Board board{board_test::fen::start};
    load(board, 4);
    const Move previous_move = find_move("e2e4");

    with_move(previous_move, [&] {
        const auto moves = ordered_moves();
        ASSERT_GE(moves.size(), 5U);
        const Move tt_move = moves[0];
        const Move killer  = moves[1];
        tt.store(position().tt_key(), tt_move, 0, 4, TTBound::UpperBound, ply());
        history().update_quiet_refutations(OrderingContext{.ply = ply()}, killer);
        store_child(tt_move, 0, 3);
        store_child(killer, 10, 3);
        store_child(moves[2], 25, 3);
        store_child(moves[3], 50, 3);
        store_child(moves[4], -200, 3);
        EXPECT_EQ(search(-200, 100, 4), 200);
        EXPECT_EQ(quiet_history(tt_move), 0);
        EXPECT_EQ(quiet_history(killer), 0);
        EXPECT_EQ(continuation_history(WHITE, PAWN, E4, tt_move), 0);
        EXPECT_EQ(continuation_history(WHITE, PAWN, E4, killer), 0);
        for (Move non_cutoff : {moves[2], moves[3]}) {
            EXPECT_LT(quiet_history(non_cutoff), 0);
            EXPECT_LT(continuation_history(WHITE, PAWN, E4, non_cutoff), 0);
        }
        EXPECT_GT(quiet_history(moves[4]), 0);
        EXPECT_GT(continuation_history(WHITE, PAWN, E4, moves[4]), 0);
        EXPECT_EQ(SearchTestAccess::counter_move_entry(history(), WHITE, PAWN, E4), moves[4]);
        EXPECT_EQ(history().context_for(position(), ply()).primary_killer, moves[4]);
    });
}

TEST_F(SearchTest, QuietPenaltiesRequireDepthAndTwoRecordedMoves) {
    for (const auto [depth, non_cutoffs] : {std::pair{3, 2}, std::pair{4, 1}}) {
        SCOPED_TRACE(depth);
        Board board{board_test::fen::start};
        board.make(Move(E2, E4));
        load(board, depth);
        const auto moves = ordered_moves();
        ASSERT_GT(moves.size(), static_cast<std::size_t>(non_cutoffs));
        for (int i = 0; i <= non_cutoffs; ++i)
            store_child(moves[i], i == non_cutoffs ? -200 : 0, depth - 1);
        EXPECT_EQ(search(-200, 100, depth), 200);
        for (int i = 0; i < non_cutoffs; ++i) {
            EXPECT_EQ(quiet_history(moves[i]), 0);
            EXPECT_EQ(continuation_history(WHITE, PAWN, E4, moves[i]), 0);
        }
    }
}

TEST_F(SearchTest, PvSearchMatchesFullWindowAndBuildsPv) {
    constexpr std::array positions{board_test::fen::start, board_test::fen::perft_position_6};
    for (const char* fen : positions) {
        SCOPED_TRACE(fen);
        Board baseline_board{fen};
        load(baseline_board, 3);
        const Score baseline = search(-scores::inf, scores::inf, 3);

        Board board{fen};
        load(board, 3);
        PV pv;
        EXPECT_EQ(pv_search(-scores::inf, scores::inf, 3, pv), baseline);
        ASSERT_FALSE(pv.empty());
        EXPECT_TRUE(position().is_legal_move(pv.front()));
    }
}

TEST_F(SearchTest, PvSearchRebuildsContinuationWithRetainedTt) {
    Board board{board_test::fen::start};
    load(board, 4);

    PV          cold_pv;
    const Score value = pv_search(-scores::inf, scores::inf, 4, cold_pv);
    ASSERT_GE(cold_pv.size(), 4);

    const auto stored = record();
    ASSERT_TRUE(stored.has_value());
    ASSERT_EQ(stored->bound, TTBound::Exact);
    ASSERT_GE(stored->depth, 4);

    PV warm_pv;
    EXPECT_EQ(pv_search(-scores::inf, scores::inf, 4, warm_pv), value);
    ASSERT_GE(warm_pv.size(), 4);

    Board replay{board};
    for (int index = 0; index < warm_pv.size(); ++index) {
        SCOPED_TRACE(index);
        const Move move = warm_pv.move_at(index);
        ASSERT_TRUE(replay.is_legal_move(move));
        replay.make(move);
    }
}

TEST_F(SearchTest, PvNodeIgnoresLowerTtBound) {
    const Board board{board_test::fen::quiet_black_to_move};
    load(board, 2);
    PV          baseline_pv;
    const Score baseline = pv_search(-scores::inf, scores::inf, 2, baseline_pv);
    const Score alpha    = baseline - 100;
    const Score beta     = baseline + 100;
    const Score bogus    = beta + 25;

    load(board, 2);
    tt.store(position().tt_key(), NULL_MOVE, bogus, 2, TTBound::LowerBound, ply());
    PV pv;
    EXPECT_EQ(pv_search(alpha, beta, 2, pv), baseline);
}

TEST_F(SearchTest, CaptureHistoryAdjustsLateMoveReductions) {
    const Board board{board_test::fen::perft_position_2};
    for (int capture_history : {-8192, 8192}) {
        SCOPED_TRACE(capture_history);
        load(board, 6);
        for (Move move : ordered_moves()) {
            // Satisfy both reduced and full-depth child searches without changing history.
            store_child(move, 100, 5);
            if (position().is_capture(move)) {
                SearchTestAccess::capture_history_entry(history(),
                                                        position().side_to_move(),
                                                        position().piece_type_on(move.from()),
                                                        move.to(),
                                                        position().captured_piece_type(move)) =
                    capture_history;
            }
        }
        tt.store(null_child_key(), NULL_MOVE, 100, 5, TTBound::Exact, 1);
        EXPECT_EQ(search(-50, 50, 6), -100);
        EXPECT_EQ(position().to_fen(), board.to_fen());
#if LATRUNCULI_SEARCH_STATS
        EXPECT_GT(counters().capture_nonzero, 0U);
        if (capture_history < 0)
            EXPECT_GT(counters().capture_increased, 0U);
        else
            EXPECT_GT(counters().capture_decreased, 0U);
#endif
    }
}

TEST_F(SearchTest, StaticEvalPathInvalidatesSkippedEvaluations) {
    Board board{board_test::fen::start};
    load(board, 2);
    auto& frames = SearchTestAccess::frames(worker);
    with_move(Move(E2, E4), [&] {
        const Score expected = eval::evaluate(position());
        (void)search(-scores::inf, scores::inf, 1);
        EXPECT_EQ(frames[ply()].static_eval, expected);
        EXPECT_TRUE(frames[ply()].real_move);

        // A PV re-search at the same ply must not retain the previous non-PV value.
        tt.clear();
        PV pv;
        (void)pv_search(-scores::inf, scores::inf, 1, pv);
        EXPECT_FALSE(frames[ply()].static_eval.has_value());

        frames[ply()].static_eval = expected;
        tt.store(position().tt_key(), NULL_MOVE, 100, 2, TTBound::Exact, ply());
        EXPECT_EQ(search(-1, 0, 2), 100);
        EXPECT_FALSE(frames[ply()].static_eval.has_value());

        frames[ply()].static_eval = expected;
        tt.clear();
        (void)search(-scores::inf, scores::inf, 0);
        EXPECT_FALSE(frames[ply()].static_eval.has_value());
    });

    with_null_move([&] {
        (void)search(-scores::inf, scores::inf, 1);
        EXPECT_FALSE(frames[ply()].real_move);
    });

    Board checked{"4k3/8/8/8/8/8/4Q3/4K3 b - - 0 1"};
    load(checked, 1);
    frames[0].static_eval = 100;
    (void)search(-scores::inf, scores::inf, 1);
    EXPECT_FALSE(frames[0].static_eval.has_value());
}

TEST_F(SearchTest, LmrResearchesAtFullDepthAfterAlphaImprovement) {
    Board baseline_board{board_test::fen::start};
    load(baseline_board, 4);
    const Score expected = search(-2000, 2000, 4);

    Board board{board_test::fen::start};
    load(board, 4);
    const auto moves = ordered_moves();
    ASSERT_GE(moves.size(), 4U);
    store_child(moves[3], -1500, 2);
    EXPECT_EQ(search(-2000, 2000, 4), expected);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().lmr_tries[0], 0U);
    EXPECT_GT(counters().lmr_researches[0], 0U);
#endif
}

TEST_F(SearchTest, DecliningLmrStillVerifiesAnApparentReducedCutoff) {
    Board board{board_test::fen::start};
    board.make(Move(E2, E4));
    board.make(Move(E7, E5));
    const auto prepare = [&] {
        load(board, 5);
        ply()                                           = 2;
        SearchTestAccess::frames(worker)[0].static_eval = eval::evaluate(position()) + 100;
        SearchTestAccess::frames(worker)[1].real_move   = true;
    };
    prepare();
    const Score expected = search(-2000, 2000, 5);
    ASSERT_LT(expected, 2000);
    prepare();
    const auto moves = ordered_moves();
    ASSERT_GE(moves.size(), 4U);
    ASSERT_FALSE(position().is_capture(moves[3]));
    ASSERT_FALSE(position().gives_check(moves[3]));
    // Depth one can satisfy the extra-reduced search, but not its depth-four verification.
    store_child(moves[3], -2500, 1);
    // The extra search can change later selective-search decisions. Check that the false
    // cutoff was rejected and the seeded child was verified at full depth, not score equality.
    EXPECT_LT(search(-2000, 2000, 5), 2000);
    with_move(moves[3], [&] {
        const auto verified = record();
        ASSERT_TRUE(verified.has_value());
        EXPECT_GE(verified->depth, 4);
        EXPECT_NE(verified->score_at_ply(ply()), -2500);
    });
}

TEST_F(SearchTest, LmrSkipsTacticalAndEvasionMoves) {
    struct Case {
        const char* fen;
        Move        tt_move;
        Move        killer;
        Move        candidate;
    };
    constexpr std::array cases{
        Case{board_test::fen::white_pawn_on_a7,
             Move(E1, D1),
             NULL_MOVE,
             Move(A7, A8, MOVE_PROM, ROOK)},
        Case{board_test::fen::checking_move_candidates, Move(A1, A8), Move(B1, G6), Move(D1, A4)},
        Case{"4k3/8/8/8/8/8/4Q3/4K3 b - - 0 1", NULL_MOVE, NULL_MOVE, NULL_MOVE},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.fen);
        Board board{tc.fen};
        load(board, 4);
        auto moves     = ordered_moves(tc.tt_move);
        Move candidate = tc.candidate;
        if (candidate.is_null()) {
            ASSERT_GE(moves.size(), 3U);
            candidate = moves[2];
        } else {
            tt.store(position().tt_key(), tc.tt_move, 0, 0, TTBound::Exact, ply());
            if (!tc.killer.is_null())
                history().update_quiet_refutations(OrderingContext{.ply = ply()}, tc.killer);
            SearchTestAccess::quiet_history_entry(
                history(), position().side_to_move(), candidate.from(), candidate.to()) = 16;
            moves = ordered_moves(tc.tt_move);
        }
        const auto it = std::find(moves.begin(), moves.end(), candidate);
        ASSERT_NE(it, moves.end());
        // Keep this test on the move-search path using the ordinary TT veto.
        tt.store(position().tt_key(), tc.tt_move, -1001, 4, TTBound::UpperBound, ply());
        for (auto move = moves.begin(); move != it; ++move)
            store_child(*move, 2100, 3);
        store_child(candidate, 900, 3);
        EXPECT_EQ(search(-2000, -1000, 4), -900);
#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().lmr_tries[0], 0U);
#endif
    }
}

TEST_F(SearchTest, StoppedSearchReturnsAlphaSentinel) {
    Board board{board_test::fen::quiet_black_to_move};
    load(board);
    SearchThreadTestAccess::request_stop(thread);
    EXPECT_EQ(search(-123, 456, 2), -123);
}

} // namespace search
