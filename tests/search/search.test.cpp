#include <algorithm>
#include <array>
#include <optional>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "core/constants.hpp"
#include "eval/evaluator.hpp"
#include "movegen/movegen.hpp"
#include "search/move_picker.hpp"
#include "search/search_worker.hpp"
#include "search/tt.hpp"
#include "support/board_fixtures.hpp"
#include "support/search_test_access.hpp"
#include "support/thread_test_access.hpp"
#include "uci/threading.hpp"
#include "uci/uci_writer.hpp"

namespace {

class SearchTest : public ::testing::Test {
protected:
    std::ostringstream output;
    uci::Writer        writer{output, output};
    ThreadPool         pool{1, writer};
    Thread&            thread{ThreadTestAccess::thread(pool)};
    SearchWorker&      worker{ThreadTestAccess::worker(thread)};
    SearchLimits       limits;

    void SetUp() override {
        limits.depth = 4;
        tt.clear();
    }

    void load(const Board& board, int depth = 4) {
        tt.clear();
        limits.depth = depth;
        worker.configure_search(board, limits, SearchClock::now());
        SearchTestAccess::reset(worker);
    }

    Board&        position() { return SearchTestAccess::board(worker); }
    int&          ply() { return SearchTestAccess::ply(worker); }
    MoveOrdering& ordering() { return SearchTestAccess::ordering(worker); }

    EvalValue search(EvalValue alpha, EvalValue beta, int depth, bool can_null = true) {
        return SearchTestAccess::alphabeta<NodeType::NonPv>(
            worker, alpha, beta, depth, nullptr, can_null);
    }

    EvalValue pv_search(EvalValue alpha, EvalValue beta, int depth, PrincipalVariation& pv) {
        return SearchTestAccess::alphabeta<NodeType::Pv>(worker, alpha, beta, depth, &pv);
    }

    Move find_move(std::string_view move_string) {
        for (Move move : movegen::generate_pseudo_legal(position())) {
            if (move.str() == move_string && position().is_legal_pseudo_move(move))
                return move;
        }
        return NULL_MOVE;
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

    std::vector<Move> legal_picker_moves(Move tt_move = NULL_MOVE) {
        std::vector<Move> moves;
        const auto        context = MoveOrdering::make_context(position());
        auto picker = move_picker::main_search(position(), ordering(), context, ply(), tt_move);
        for (Move move = picker.next(); !move.is_null(); move = picker.next()) {
            if (position().is_legal_pseudo_move(move))
                moves.push_back(move);
        }
        return moves;
    }

    void store_child(Move move, EvalValue score, int depth, TTBound bound = TTBound::Exact) {
        with_move(move, [&] { tt.store(position().key(), NULL_MOVE, score, depth, bound, ply()); });
    }

    PositionKey null_child_key() {
        Board copy{position().to_fen()};
        copy.make_null();
        return copy.key();
    }

    PositionKey descendant_null_key(Move move) {
        Board copy{position().to_fen()};
        copy.make(move);
        copy.make_null();
        return copy.key();
    }

    std::optional<TTRecord> record() const {
        return tt.probe(SearchTestAccess::board(worker).key());
    }

    int quiet_history(Move move) {
        return ordering().quiets.get(position().side_to_move(), move.from(), move.to());
    }

    int continuation_history(Color prev_c, PieceType prev_piece, Square prev_to, Move move) {
        return ordering().continuations.get(
            prev_c, prev_piece, prev_to, position().piece_type_on(move.from()), move.to());
    }

#if LATRUNCULI_SEARCH_STATS
    const SearchCounters& counters() {
        return SearchTestAccess::instrumentation(worker).raw_counters();
    }
#endif
};

} // namespace

TEST_F(SearchTest, HandlesDrawAndMaxPlyExits) {
    Board drawn{"k7/8/2K5/8/8/8/8/8 b - - 100 1"};
    load(drawn);
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf, 2), eval_value::draw);

    Board max_ply{board_test::fen::quiet_black_to_move};
    load(max_ply);
    ply() = engine::max_search_ply;
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf, 1), evaluate(position()));

    Board drawn_max{"k7/8/8/8/8/8/4r3/K2Q4 w - - 100 1"};
    load(drawn_max);
    ply() = engine::max_search_ply;
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf, 1), eval_value::draw);
}

TEST_F(SearchTest, ReturnsFailSoftValues) {
    constexpr auto one_evasion = board_test::fen::one_legal_evasion;
    constexpr int  depth       = 1;
    const Move     evasion{A8, B8};

    struct Case {
        const char* name;
        EvalValue   alpha;
        EvalValue   beta;
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
        const EvalValue expected =
            -with_move(evasion, [&] { return search(-tc.beta, -tc.alpha, depth - 1); });

        Board board{one_evasion};
        load(board, depth);
        const EvalValue actual = search(tc.alpha, tc.beta, depth);
        EXPECT_EQ(actual, expected);
        EXPECT_NE(actual, tc.name == std::string_view{"fail low"} ? tc.alpha : tc.beta);
    }
}

TEST_F(SearchTest, AppliesMateDistanceBounds) {
    Board board{board_test::fen::quiet_black_to_move};
    load(board);
    ply() = 4;

    constexpr EvalValue lower = -eval_value::mate + 4;
    EXPECT_EQ(search(-eval_value::inf, lower - 1, 1), lower);

    load(board);
    ply()                     = 4;
    constexpr EvalValue upper = eval_value::mate - 5;
    EXPECT_EQ(search(upper + 1, eval_value::inf, 1), upper + 1);
}

TEST_F(SearchTest, UsesOnlyDepthEligibleTtBounds) {
    struct Case {
        const char* name;
        TTBound     bound;
        EvalValue   score;
        EvalValue   alpha;
        EvalValue   beta;
    };

    constexpr std::array cases{
        Case{"exact", TTBound::Exact, 321, -eval_value::inf, eval_value::inf},
        Case{"lower", TTBound::LowerBound, 500, 100, 200},
        Case{"upper", TTBound::UpperBound, -500, -200, -100},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{board_test::fen::quiet_black_to_move};
        load(board, 2);
        tt.store(position().key(), NULL_MOVE, tc.score, 2, tc.bound, ply());
        EXPECT_EQ(search(tc.alpha, tc.beta, 2), tc.score);

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().main_tt_cutoffs[0], 1U);
        EXPECT_EQ(counters().q_tt_probes[0], 0U);
#endif
    }

    Board baseline_board{board_test::fen::quiet_black_to_move};
    load(baseline_board, 2);
    const EvalValue baseline = search(-eval_value::inf, eval_value::inf, 2);

    Board shallow_board{board_test::fen::quiet_black_to_move};
    load(shallow_board, 2);
    tt.store(position().key(), Move(H1, H2), baseline + 500, 1, TTBound::Exact, ply());
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf, 2), baseline);
}

TEST_F(SearchTest, StoresWindowClassifiedTtBounds) {
    Board exact_board{board_test::fen::quiet_black_to_move};
    load(exact_board, 2);
    const EvalValue exact  = search(-eval_value::inf, eval_value::inf, 2);
    auto            stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->score_at_ply(ply()), exact);
    EXPECT_EQ(stored->depth, 2);
    EXPECT_EQ(stored->bound, TTBound::Exact);

    Board lower_source{board_test::fen::one_legal_evasion};
    load(lower_source, 1);
    const EvalValue full = search(-eval_value::inf, eval_value::inf, 1);

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
    EXPECT_EQ(search(-eval_value::inf, eval_value::inf, 2), -eval_value::mate + 3);
    stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->score, -eval_value::mate);
    EXPECT_EQ(stored->bound, TTBound::Exact);
    EXPECT_EQ(stored->move, NULL_MOVE);
}

TEST_F(SearchTest, NullMovePruningReturnsFailSoftCutoff) {
    Board board{board_test::fen::start};
    load(board, 4);
    const PositionKey root_key = position().key();
    tt.store(null_child_key(), NULL_MOVE, -200, 1, TTBound::Exact, 1);

    EXPECT_EQ(search(-50, 50, 4), 200);
    EXPECT_FALSE(tt.probe(root_key).has_value());

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().null_move_tries[0], 1U);
    EXPECT_EQ(counters().null_move_cutoffs[0], 1U);
#endif
}

TEST_F(SearchTest, PvNodesDoNotUseNullMovePruning) {
    Board baseline_board{board_test::fen::start};
    load(baseline_board, 4);
    PrincipalVariation baseline_pv;
    const EvalValue    baseline = pv_search(-50, 50, 4, baseline_pv);

    Board board{board_test::fen::start};
    load(board, 4);
    tt.store(null_child_key(), NULL_MOVE, -200, 1, TTBound::Exact, 1);
    PrincipalVariation pv;
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
        bool        can_null;
    };

    constexpr std::array cases{
        Case{"in check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", 4, true},
        Case{"insufficient material", board_test::fen::quiet_black_to_move, 4, true},
        Case{"lone rook", "4k3/8/8/8/8/8/8/4K2R w - - 0 1", 4, true},
        Case{"shallow", board_test::fen::start, 2, true},
        Case{"repeated null", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 1", 4, false},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board baseline_board{tc.fen};
        load(baseline_board, tc.depth);
        const EvalValue baseline = search(-50, 50, tc.depth, tc.can_null);

        Board board{tc.fen};
        load(board, tc.depth);
        tt.store(null_child_key(), NULL_MOVE, -200, std::max(0, tc.depth - 3), TTBound::Exact, 1);
        EXPECT_EQ(search(-50, 50, tc.depth, tc.can_null), baseline);

#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().null_move_tries[0], 0U);
#endif
    }
}

TEST_F(SearchTest, NullMovePruningHonorsParentUpperBoundVeto) {
    Board baseline_board{board_test::fen::start};
    load(baseline_board, 4);
    tt.store(position().key(), NULL_MOVE, 49, 4, TTBound::UpperBound, ply());
    tt.store(null_child_key(), NULL_MOVE, -200, 1, TTBound::Exact, 1);
    const EvalValue baseline = search(-50, 50, 4, false);

    Board board{board_test::fen::start};
    load(board, 4);
    tt.store(position().key(), NULL_MOVE, 49, 4, TTBound::UpperBound, ply());
    tt.store(null_child_key(), NULL_MOVE, -200, 1, TTBound::Exact, 1);
    EXPECT_EQ(search(-50, 50, 4), baseline);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().null_move_tries[0], 0U);
#endif
}

TEST_F(SearchTest, NullMoveReenablesAfterARealDescendantMove) {
    Board board{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 1"};
    load(board, 5);
    const Move real_move = find_move("e7e5");
    ASSERT_FALSE(real_move.is_null());
    const PositionKey immediate  = null_child_key();
    const PositionKey descendant = descendant_null_key(real_move);

    tt.store(position().key(), real_move, 0, 0, TTBound::LowerBound, ply());
    (void)search(-50, 50, 5, false);

    EXPECT_FALSE(tt.probe(immediate).has_value());
    EXPECT_TRUE(tt.probe(descendant).has_value());
}

TEST_F(SearchTest, RazoringReturnsQsearchFailLowWithoutParentTtStore) {
    Board board{board_test::fen::quiet_black_to_move};
    load(board, 2);
    const EvalValue static_eval = evaluate(position());
    EXPECT_EQ(search(static_eval + 901, static_eval + 1001, 2), static_eval);
    const auto stored = record();
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(stored->depth, 0);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().razor_cutoffs[0], 1U);
#endif
}

TEST_F(SearchTest, RazoringRequiresAllGuards) {
    struct Case {
        const char* name;
        const char* fen;
        int         depth;
        bool        can_null;
        bool        pv;
        bool        seed_tt;
        int         alpha_offset;
    };

    constexpr std::array cases{
        Case{"PV", board_test::fen::quiet_black_to_move, 2, true, true, false, 901},
        Case{"check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", 2, true, false, false, 901},
        Case{"deep", board_test::fen::quiet_black_to_move, 4, true, false, false, 1901},
        Case{"null disabled", board_test::fen::start, 2, false, false, false, 901},
        Case{"TT move", board_test::fen::start, 2, true, false, true, 901},
        Case{"inside margin", board_test::fen::start, 2, true, false, false, 899},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{tc.fen};
        load(board, tc.depth);
        if (tc.seed_tt) {
            const Move move = legal_picker_moves().front();
            tt.store(position().key(), move, 0, 0, TTBound::Exact, ply());
        }
        const EvalValue alpha = evaluate(position()) + tc.alpha_offset;
        if (tc.pv) {
            PrincipalVariation pv;
            (void)pv_search(alpha, alpha + 100, tc.depth, pv);
        } else {
            (void)search(alpha, alpha + 100, tc.depth, tc.can_null);
        }
        ASSERT_TRUE(record().has_value());
        EXPECT_EQ(record()->depth, tc.depth);
    }
}

TEST_F(SearchTest, FutilitySkipsOnlyAfterFirstLegalQuiet) {
    Board expected_board{board_test::fen::start};
    load(expected_board, 2);
    const EvalValue alpha    = evaluate(position()) + 401;
    const EvalValue beta     = alpha + 100;
    const Move      first    = legal_picker_moves().front();
    const EvalValue expected = with_move(first, [&] { return -search(-beta, -alpha, 1); });

    Board board{board_test::fen::start};
    load(board, 2);
    EXPECT_EQ(search(alpha, beta, 2), expected);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_EQ(counters().futility_skips[0], 1U);
#endif
}

TEST_F(SearchTest, FutilityRequiresAllGuards) {
    Board           eval_board{board_test::fen::start};
    const EvalValue static_eval = evaluate(eval_board);
    struct Case {
        const char* name;
        const char* fen;
        int         depth;
        EvalValue   alpha;
        bool        pv;
    };
    const std::array cases{
        Case{"PV", board_test::fen::start, 2, static_eval + 401, true},
        Case{"check", "k7/8/2K5/8/8/8/R6q/8 b - - 0 1", 2, 1000, false},
        Case{"deep", board_test::fen::start, 4, static_eval + 401, false},
        Case{"mate alpha", board_test::fen::start, 2, eval_value::mate_bound, false},
        Case{"inside margin", board_test::fen::start, 2, static_eval + 399, false},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.name);
        Board board{tc.fen};
        load(board, tc.depth);
        if (tc.pv) {
            PrincipalVariation pv;
            (void)pv_search(tc.alpha, tc.alpha + 100, tc.depth, pv);
        } else {
            (void)search(tc.alpha, tc.alpha + 100, tc.depth, false);
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
        const EvalValue alpha = evaluate(position()) + 401;
        const EvalValue beta  = alpha + 1000;
        tt.store(position().key(), tc.first, 0, 0, TTBound::Exact, ply());
        if (tc.killer)
            ordering().killers.update(tc.tactical, ply());
        store_child(tc.tactical, -(beta + 100), 1);
        EXPECT_EQ(search(alpha, beta, 2), beta + 100);
    }
}

TEST_F(SearchTest, QuietCutoffUpdatesPreviousMoveContext) {
    Board board{board_test::fen::start};
    load(board, 2);
    const Move previous = find_move("e2e4");

    with_move(previous, [&] {
        const Move cutoff = legal_picker_moves().front();
        tt.store(position().key(), cutoff, 0, 0, TTBound::UpperBound, ply());
        store_child(cutoff, -200, 1);
        EXPECT_EQ(search(-200, 100, 2, false), 200);
        EXPECT_GT(quiet_history(cutoff), 0);
        EXPECT_GT(continuation_history(WHITE, PAWN, E4, cutoff), 0);
        EXPECT_EQ(ordering().counters.get(WHITE, PAWN, E4), cutoff);
    });
}

TEST_F(SearchTest, QuietCutoffWithoutRealPreviousMoveSkipsContextUpdates) {
    const auto run_case = [&](bool after_null) {
        Board board{board_test::fen::start};
        load(board, 2);
        const auto test = [&] {
            const Move cutoff = legal_picker_moves().front();
            store_child(cutoff, -200, 1);
            EXPECT_EQ(search(-200, 100, 2, false), 200);
            EXPECT_EQ(ordering().counters.get(WHITE, PAWN, E4), NULL_MOVE);
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

TEST_F(SearchTest, NonQuietCutoffSkipsRefutationUpdates) {
    struct Case {
        const char* fen;
        Move        previous;
        Move        cutoff;
    };
    constexpr std::array cases{
        Case{"r3k3/B7/8/8/8/8/8/4K3 w - - 0 1", Move(E1, D1), Move(A8, A7)},
        Case{"4k3/8/8/8/8/8/p7/4K3 w - - 0 1", Move(E1, D1), Move(A2, A1, MOVE_PROM, QUEEN)},
    };

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.fen);
        Board board{tc.fen};
        load(board, 2);
        with_move(tc.previous, [&] {
            tt.store(position().key(), tc.cutoff, 0, 0, TTBound::UpperBound, ply());
            store_child(tc.cutoff, -200, 1);
            EXPECT_EQ(search(-200, 100, 2, false), 200);
        });
        EXPECT_EQ(ordering().counters.get(WHITE, KING, D1), NULL_MOVE);
    }
}

TEST_F(SearchTest, QuietMalusPenalizesEligibleMovesInBothHistories) {
    Board board{board_test::fen::start};
    load(board, 4);
    const Move previous = find_move("e2e4");

    with_move(previous, [&] {
        const auto moves = legal_picker_moves();
        ASSERT_GE(moves.size(), 3U);
        store_child(moves[0], 0, 3);
        store_child(moves[1], 50, 3);
        store_child(moves[2], -200, 3);
        EXPECT_EQ(search(-200, 100, 4, false), 200);
        for (Move failed : {moves[0], moves[1]}) {
            EXPECT_LT(quiet_history(failed), 0);
            EXPECT_LT(continuation_history(WHITE, PAWN, E4, failed), 0);
        }
        EXPECT_GT(quiet_history(moves[2]), 0);
        EXPECT_GT(continuation_history(WHITE, PAWN, E4, moves[2]), 0);
    });
}

TEST_F(SearchTest, QuietMalusExcludesTtAndKillerHints) {
    Board board{board_test::fen::start};
    load(board, 4);
    const Move previous = find_move("e2e4");

    with_move(previous, [&] {
        const auto moves = legal_picker_moves();
        ASSERT_GE(moves.size(), 5U);
        const Move tt_move = moves[0];
        const Move killer  = moves[1];
        tt.store(position().key(), tt_move, 0, 4, TTBound::UpperBound, ply());
        ordering().killers.update(killer, ply());
        store_child(tt_move, 0, 3);
        store_child(killer, 10, 3);
        store_child(moves[2], 25, 3);
        store_child(moves[3], 50, 3);
        store_child(moves[4], -200, 3);
        EXPECT_EQ(search(-200, 100, 4, false), 200);
        EXPECT_EQ(quiet_history(tt_move), 0);
        EXPECT_EQ(quiet_history(killer), 0);
        EXPECT_EQ(continuation_history(WHITE, PAWN, E4, tt_move), 0);
        EXPECT_EQ(continuation_history(WHITE, PAWN, E4, killer), 0);
        EXPECT_LT(quiet_history(moves[2]), 0);
        EXPECT_LT(continuation_history(WHITE, PAWN, E4, moves[2]), 0);
    });
}

TEST_F(SearchTest, QuietMalusRequiresDepthAndTwoFailedMoves) {
    {
        Board board{board_test::fen::start};
        load(board, 3);
        const Move previous = find_move("e2e4");
        with_move(previous, [&] {
            const auto moves = legal_picker_moves();
            ASSERT_GE(moves.size(), 3U);
            store_child(moves[0], 0, 2);
            store_child(moves[1], 25, 2);
            store_child(moves[2], -200, 2);
            EXPECT_EQ(search(-200, 100, 3, false), 200);
            for (int i = 0; i < 2; ++i) {
                EXPECT_EQ(quiet_history(moves[i]), 0);
                EXPECT_EQ(continuation_history(WHITE, PAWN, E4, moves[i]), 0);
            }
        });
    }

    {
        Board board{board_test::fen::start};
        load(board, 4);
        const auto moves = legal_picker_moves();
        ASSERT_GE(moves.size(), 2U);
        store_child(moves[0], 0, 3);
        store_child(moves[1], -200, 3);
        EXPECT_EQ(search(-200, 100, 4, false), 200);
        EXPECT_EQ(quiet_history(moves[0]), 0);
    }
}

TEST_F(SearchTest, PvSearchMatchesFullWindowAndBuildsPv) {
    constexpr std::array positions{board_test::fen::start, board_test::fen::perft_position_6};
    for (const char* fen : positions) {
        SCOPED_TRACE(fen);
        Board baseline_board{fen};
        load(baseline_board, 3);
        const EvalValue baseline = search(-eval_value::inf, eval_value::inf, 3);

        Board board{fen};
        load(board, 3);
        PrincipalVariation pv;
        EXPECT_EQ(pv_search(-eval_value::inf, eval_value::inf, 3, pv), baseline);
        ASSERT_FALSE(pv.empty());
        EXPECT_TRUE(position().is_legal_move(pv.front()));
    }
}

TEST_F(SearchTest, PvNodesIgnoreNonExactMainTtBounds) {
    Board baseline_board{board_test::fen::quiet_black_to_move};
    load(baseline_board, 2);
    PrincipalVariation baseline_pv;
    const EvalValue    baseline = pv_search(-eval_value::inf, eval_value::inf, 2, baseline_pv);
    const EvalValue    alpha    = baseline - 100;
    const EvalValue    beta     = baseline + 100;
    const EvalValue    bogus    = beta + 25;

    Board non_pv_board{board_test::fen::quiet_black_to_move};
    load(non_pv_board, 2);
    tt.store(position().key(), NULL_MOVE, bogus, 2, TTBound::LowerBound, ply());
    EXPECT_EQ(search(alpha, beta, 2), bogus);

    Board pv_board{board_test::fen::quiet_black_to_move};
    load(pv_board, 2);
    tt.store(position().key(), NULL_MOVE, bogus, 2, TTBound::LowerBound, ply());
    PrincipalVariation pv;
    EXPECT_EQ(pv_search(alpha, beta, 2, pv), baseline);
}

TEST_F(SearchTest, LmrResearchesAtFullDepthAfterAlphaImprovement) {
    Board baseline_board{board_test::fen::start};
    load(baseline_board, 4);
    const EvalValue expected = search(-2000, 2000, 4, false);

    Board board{board_test::fen::start};
    load(board, 4);
    const auto moves = legal_picker_moves();
    ASSERT_GE(moves.size(), 4U);
    store_child(moves[3], -1500, 2);
    EXPECT_EQ(search(-2000, 2000, 4, false), expected);

#if LATRUNCULI_SEARCH_STATS
    EXPECT_GT(counters().lmr_tries[0], 0U);
    EXPECT_GT(counters().lmr_researches[0], 0U);
#endif
}

TEST_F(SearchTest, LmrRequiresDepthAndLateMove) {
    struct Case {
        const char* fen;
        int         depth;
    };
    constexpr std::array cases{
        Case{board_test::fen::start, 2},
        Case{"1k6/8/2K5/8/8/8/8/8 b - - 0 1", 4},
    };
    for (const auto& tc : cases) {
        Board board{tc.fen};
        load(board, tc.depth);
        (void)search(-2000, 2000, tc.depth, false);
#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().lmr_tries[0], 0U);
#endif
    }
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
        auto moves     = legal_picker_moves(tc.tt_move);
        Move candidate = tc.candidate;
        if (candidate.is_null()) {
            ASSERT_GE(moves.size(), 3U);
            candidate = moves[2];
        } else {
            tt.store(position().key(), tc.tt_move, 0, 0, TTBound::Exact, ply());
            if (!tc.killer.is_null())
                ordering().killers.update(tc.killer, ply());
            ordering().quiets.reward(
                position().side_to_move(), candidate.from(), candidate.to(), 4);
            moves = legal_picker_moves(tc.tt_move);
        }
        const auto it = std::find(moves.begin(), moves.end(), candidate);
        ASSERT_NE(it, moves.end());
        for (auto move = moves.begin(); move != it; ++move)
            store_child(*move, 2100, 3);
        store_child(candidate, 900, 3);
        EXPECT_EQ(search(-2000, -1000, 4, false), -900);
#if LATRUNCULI_SEARCH_STATS
        EXPECT_EQ(counters().lmr_tries[0], 0U);
#endif
    }
}

TEST_F(SearchTest, StoppedSearchReturnsAlphaSentinel) {
    Board board{board_test::fen::quiet_black_to_move};
    load(board);
    ThreadTestAccess::request_stop(thread);
    EXPECT_EQ(search(-123, 456, 2), -123);
}
