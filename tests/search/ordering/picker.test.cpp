#include "search/ordering/picker.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "board/board.hpp"
#include "board/see.hpp"
#include "movegen/generator.hpp"
#include "search/limits.hpp"
#include "search/ordering/capture_scoring.hpp"
#include "search/ordering/state.hpp"
#include "support/board_fixtures.hpp"
#include "support/see_oracle.hpp"

namespace search::ordering {

class PickerTest : public ::testing::Test {
protected:
    int           ply = 5;
    Board         board{board_test::fen::perft_position_3};
    State         state;
    KillerMoves&  killers       = state.killers;
    QuietHistory& quiet_history = state.quiets;
};

namespace {

constexpr std::string_view CHECK_BY_PREVIOUS_MOVE_FEN = "k7/8/2K5/8/8/8/8/R7 w - - 0 1";
constexpr std::string_view WEAK_CAPTURE_FEN           = "2b3k1/3p4/8/8/8/8/8/3Q2K1 w - - 0 1";
constexpr std::string_view PROMOTION_AND_CAPTURE_FEN  = "4k3/P7/8/1p6/3N4/8/8/4K3 w - - 0 1";

std::vector<MoveBits> sorted_move_bits(const movegen::MoveList& movelist) {
    std::vector<MoveBits> bits;
    for (const Move move : movelist)
        bits.push_back(move.bits);
    std::sort(bits.begin(), bits.end());
    return bits;
}

std::vector<MoveBits> sorted_move_bits(const std::vector<Move>& moves) {
    std::vector<MoveBits> bits;
    for (const Move move : moves)
        bits.push_back(move.bits);
    std::sort(bits.begin(), bits.end());
    return bits;
}

std::vector<Move> collect_moves(ordering::Picker& picker) {
    std::vector<Move> moves;
    moves.reserve(movegen::MoveList::capacity);
    for (Move move = picker.next(); !move.is_null(); move = picker.next())
        moves.push_back(move);
    return moves;
}

void seed_counter_hint(const State::Context& context, State& state, Move counter) {
    if (!counter.is_null() && context.has_previous_move)
        state.counters.update(
            context.previous_side, context.previous_piece, context.previous_to, counter);
}

void boost_continuation_hint(const Board&          board,
                             const State::Context& context,
                             State&                state,
                             Move                  move) {
    if (!context.has_previous_move)
        return;

    const PieceType piece = board.piece_type_on(move.from());
    if (piece == NO_PIECETYPE)
        return;

    for (int i = 0; i < 8; ++i) {
        state.continuations.reward(context.previous_side,
                                   context.previous_piece,
                                   context.previous_to,
                                   piece,
                                   move.to(),
                                   Limits::max_depth);
    }
}

std::vector<Move> picked_main_search(
    const Board& board, State& state, int ply, Move tt_move = NULL_MOVE, Move counter = NULL_MOVE) {
    const auto context = State::make_context(board);
    seed_counter_hint(context, state, counter);
    auto picker = Picker::for_main_search(board, state, context, ply, tt_move);
    return collect_moves(picker);
}

std::vector<Move> picked_qsearch(const Board& board, State& state, Move tt_move = NULL_MOVE) {
    auto picker = Picker::for_quiescence(board, state, tt_move);
    return collect_moves(picker);
}

void expect_hash_move_first_once(const std::vector<Move>& moves, Move expected) {
    ASSERT_FALSE(moves.empty());
    EXPECT_EQ(moves.front(), expected);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), expected), 1);
}

bool expected_qsearch_noisy(const Board& board, Move move) {
    return move.type() == MOVE_PROM
        || board_test::exchange_oracle(board, move, see::piece_values) >= 0;
}

} // namespace

TEST_F(PickerTest, MainSearchReturnsEveryPseudoLegalMoveOnce) {
    for (std::string_view fen : {std::string_view{board_test::fen::start},
                                 std::string_view{board_test::fen::perft_position_2},
                                 std::string_view{board_test::fen::perft_position_3},
                                 std::string_view{board_test::fen::legal_en_passant_a3},
                                 std::string_view{board_test::fen::promotion_options},
                                 std::string_view{board_test::fen::capture_promotion},
                                 std::string_view{board_test::fen::castling}}) {
        SCOPED_TRACE(fen);
        Board position{fen};
        ASSERT_FALSE(position.is_check());

        const auto picked    = picked_main_search(position, state, ply);
        const auto generated = movegen::generate_pseudo_legal(position);
        EXPECT_EQ(sorted_move_bits(picked), sorted_move_bits(generated));
    }
}

TEST_F(PickerTest, MainSearchOrdersTtCaptureRefutationsAndHistories) {
    const Move tt_move{E2, E3};
    const Move capture{B4, F4};
    const Move killer{A5, A4};
    const Move saturated{A5, A6};
    const Move lightly_scored{B4, D4};

    killers.update(killer, ply);
    quiet_history.reward(board.side_to_move(), lightly_scored.from(), lightly_scored.to(), 1);
    for (int i = 0; i < 8; ++i) {
        quiet_history.reward(
            board.side_to_move(), saturated.from(), saturated.to(), Limits::max_depth);
    }

    const auto moves = picked_main_search(board, state, ply, tt_move);

    ASSERT_GE(moves.size(), 5U);
    EXPECT_EQ(moves[0], tt_move);
    EXPECT_EQ(moves[1], capture);
    EXPECT_EQ(moves[2], killer);
    EXPECT_EQ(moves[3], saturated);
    EXPECT_EQ(moves[4], lightly_scored);
}

TEST_F(PickerTest, MainSearchOrdersKillersCounterAndContinuationHistory) {
    Board position{board_test::fen::start};
    position.make(Move(E2, E4));
    position.make(Move(G8, F6));

    const Move killer_1{D2, D4};
    const Move killer_2{G1, F3};
    const Move counter{B1, C3};
    const Move boosted{F1, E2};
    const Move ordinary{G1, E2};

    killers.update(killer_2, ply);
    killers.update(killer_1, ply);
    const auto context = State::make_context(position);
    boost_continuation_hint(position, context, state, boosted);

    const auto moves = picked_main_search(position, state, ply, NULL_MOVE, counter);

    ASSERT_GE(moves.size(), 5U);
    EXPECT_EQ(moves[0], killer_1);
    EXPECT_EQ(moves[1], killer_2);
    EXPECT_EQ(moves[2], counter);
    EXPECT_EQ(moves[3], boosted);
    EXPECT_EQ(moves[4], ordinary);
}

TEST_F(PickerTest, MainSearchContinuationHistoryRequiresMatchingPreviousMove) {
    Board position{board_test::fen::start};
    position.make(Move(E2, E4));

    const auto baseline = picked_main_search(position, state, ply);
    ASSERT_GE(baseline.size(), 2U);

    const Move      target = baseline[1];
    const PieceType piece  = position.piece_type_on(target.from());
    for (int i = 0; i < 8; ++i) {
        state.continuations.reward(BLACK, PAWN, D4, piece, target.to(), Limits::max_depth);
    }

    EXPECT_EQ(picked_main_search(position, state, ply), baseline);
}

TEST_F(PickerTest, MainSearchPrioritizesAndDeduplicatesHintsDeterministically) {
    Board position{board_test::fen::start};
    position.make(Move(E2, E4));
    position.make(Move(G8, F6));
    const Move hint{G1, F3};

    killers.update(hint, ply);
    const auto first  = picked_main_search(position, state, ply, hint, hint);
    const auto second = picked_main_search(position, state, ply, hint, hint);

    expect_hash_move_first_once(first, hint);
    EXPECT_EQ(second, first);
}

TEST_F(PickerTest, MainSearchValidatesCounterHintsWithoutReclassifyingTacticals) {
    {
        SCOPED_TRACE("stale counter");
        Board position{board_test::fen::after_e2e4};
        position.make(Move(G8, F6));
        const Move stale{A1, A3};

        const auto moves = picked_main_search(position, state, ply, NULL_MOVE, stale);
        EXPECT_EQ(std::count(moves.begin(), moves.end(), stale), 0);
    }
    {
        SCOPED_TRACE("capture counter");
        Board position{"2b3k1/3p4/8/8/8/8/8/3Q2K1 b - - 0 1"};
        position.make(Move(G8, F8));
        const Move capture{D1, D7};

        const auto moves      = picked_main_search(position, state, ply, NULL_MOVE, capture);
        const auto capture_it = std::find(moves.begin(), moves.end(), capture);
        const auto quiet_it   = std::find_if(
            moves.begin(), moves.end(), [&](Move move) { return !position.is_capture(move); });
        ASSERT_NE(capture_it, moves.end());
        ASSERT_NE(quiet_it, moves.end());
        EXPECT_LT(quiet_it, capture_it);
    }
    {
        SCOPED_TRACE("promotion counter");
        Board position{"4k3/P6p/8/8/8/8/p6P/4K3 b - - 0 1"};
        position.make(Move(E8, D8));
        const Move promotion{A7, A8, MOVE_PROM, QUEEN};

        const auto moves = picked_main_search(position, state, ply, NULL_MOVE, promotion);
        EXPECT_EQ(std::count(moves.begin(), moves.end(), promotion), 1);
    }
}

TEST_F(PickerTest, MainSearchInCheckReturnsEvasionsAndIgnoresNonEvasionHints) {
    {
        SCOPED_TRACE("non-evasion TT hint");
        Board      position{board_test::fen::one_legal_evasion};
        const Move non_evasion{A8, A7};
        ASSERT_TRUE(position.is_pseudo_legal(non_evasion));
        ASSERT_FALSE(position.is_legal_pseudo_move(non_evasion));

        const auto baseline = picked_main_search(position, state, ply);
        EXPECT_EQ(sorted_move_bits(baseline),
                  sorted_move_bits(movegen::generate_evasions(position)));
        EXPECT_EQ(picked_main_search(position, state, ply, non_evasion), baseline);
    }
    {
        SCOPED_TRACE("counter hint");
        Board position{"k7/8/2K5/8/8/8/1R6/8 w - - 0 1"};
        position.make(Move(B2, A2));
        ASSERT_TRUE(position.is_check());
        const Move counter{A8, B8};

        const auto baseline = picked_main_search(position, state, ply);
        EXPECT_EQ(picked_main_search(position, state, ply, NULL_MOVE, counter), baseline);
    }
}

TEST_F(PickerTest, MainSearchOrdersPromotionsAndWeakCapturesByStage) {
    {
        SCOPED_TRACE("promotion before capture");
        Board      position{PROMOTION_AND_CAPTURE_FEN};
        const Move promotion{A7, A8, MOVE_PROM, QUEEN};
        const Move capture{D4, B5};

        const auto moves        = picked_main_search(position, state, ply);
        const auto promotion_it = std::find(moves.begin(), moves.end(), promotion);
        const auto capture_it   = std::find(moves.begin(), moves.end(), capture);
        ASSERT_NE(promotion_it, moves.end());
        ASSERT_NE(capture_it, moves.end());
        EXPECT_LT(promotion_it, capture_it);
    }
    {
        SCOPED_TRACE("quiet before weak capture");
        Board      position{WEAK_CAPTURE_FEN};
        const Move weak_capture{D1, D7};

        const auto moves    = picked_main_search(position, state, ply);
        const auto weak_it  = std::find(moves.begin(), moves.end(), weak_capture);
        const auto quiet_it = std::find_if(moves.begin(), moves.end(), [&](Move move) {
            return move.type() != MOVE_PROM && !position.is_capture(move);
        });
        ASSERT_NE(weak_it, moves.end());
        ASSERT_NE(quiet_it, moves.end());
        EXPECT_LT(quiet_it, weak_it);
    }
}

TEST_F(PickerTest, QSearchReturnsOnlyNonLosingNoisyMoves) {
    for (std::string_view fen : {std::string_view{board_test::fen::perft_position_2},
                                 std::string_view{board_test::fen::perft_position_3},
                                 std::string_view{board_test::fen::legal_en_passant_a3},
                                 std::string_view{board_test::fen::promotion_options},
                                 std::string_view{board_test::fen::capture_promotion},
                                 std::string_view{"1rr4k/P7/8/8/8/8/8/K7 w - - 0 1"},
                                 WEAK_CAPTURE_FEN}) {
        SCOPED_TRACE(fen);
        Board position{fen};
        ASSERT_FALSE(position.is_check());

        const auto        moves = picked_qsearch(position, state);
        movegen::MoveList expected;
        for (const Move move : movegen::generate_noisy(position)) {
            if (expected_qsearch_noisy(position, move))
                expected.add(move);
        }

        EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(expected));
        for (const Move move : moves) {
            EXPECT_TRUE(move.type() == MOVE_PROM || position.is_capture(move)) << move.str();
            EXPECT_TRUE(expected_qsearch_noisy(position, move)) << move.str();
        }
    }
}

TEST_F(PickerTest, LosingTtCaptureIsStillReturnedFirstOnce) {
    const Board position{WEAK_CAPTURE_FEN};
    const Move  capture{D1, D7};
    ASSERT_LT(position.see(capture), 0);

    expect_hash_move_first_once(picked_qsearch(position, state, capture), capture);
    expect_hash_move_first_once(picked_main_search(position, state, ply, capture), capture);
}

TEST_F(PickerTest, CaptureEvasionsKeepNumericalSeeOrdering) {
    const Board position{"1r5k/8/8/8/1b6/P7/2N5/1Q2K3 w - - 0 1"};
    ASSERT_TRUE(position.is_check());
    const Move pawn_capture{A3, B4};
    const Move knight_capture{C2, B4};
    const Move queen_capture{B1, B4};
    ASSERT_EQ(position.see(pawn_capture), position.see(knight_capture));
    ASSERT_GE(position.see(knight_capture), 0);
    ASSERT_LT(position.see(queen_capture), 0);

    const auto main_moves = picked_main_search(position, state, ply);
    ASSERT_GE(main_moves.size(), 3U);
    EXPECT_EQ(main_moves[0], knight_capture);
    EXPECT_EQ(main_moves[1], pawn_capture);
    EXPECT_EQ(picked_qsearch(position, state), main_moves);
    EXPECT_EQ(sorted_move_bits(main_moves), sorted_move_bits(movegen::generate_evasions(position)));
}

TEST_F(PickerTest, QSearchValidatesAndPrioritizesTtHints) {
    {
        SCOPED_TRACE("capture");
        const Move capture{B4, F4};
        expect_hash_move_first_once(picked_qsearch(board, state, capture), capture);
    }
    {
        SCOPED_TRACE("promotion");
        Board      position{board_test::fen::promotion_options};
        const Move promotion{A7, A8, MOVE_PROM, QUEEN};
        expect_hash_move_first_once(picked_qsearch(position, state, promotion), promotion);
    }
    {
        SCOPED_TRACE("quiet rejection");
        const Move quiet{E2, E3};
        const auto moves = picked_qsearch(board, state, quiet);
        EXPECT_EQ(std::find(moves.begin(), moves.end(), quiet), moves.end());
    }
}

TEST_F(PickerTest, QSearchInCheckReturnsEvasionsAndPrioritizesLegalTtHint) {
    Board      position{board_test::fen::one_legal_evasion};
    const Move quiet_evasion{A8, B8};
    ASSERT_TRUE(position.is_legal_pseudo_move(quiet_evasion));

    const auto moves = picked_qsearch(position, state, quiet_evasion);
    expect_hash_move_first_once(moves, quiet_evasion);
    EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(movegen::generate_evasions(position)));
}

TEST_F(PickerTest, QSearchAndEvasionsIgnoreContinuationHistory) {
    Board      position{CHECK_BY_PREVIOUS_MOVE_FEN};
    const Move checking_move{A1, A2};
    const Move quiet_evasion{A8, B8};
    ASSERT_TRUE(position.is_legal_move(checking_move));

    position.make(checking_move);
    ASSERT_TRUE(position.is_check());
    ASSERT_TRUE(position.is_legal_pseudo_move(quiet_evasion));

    const auto main_baseline = picked_main_search(position, state, ply);
    const auto q_baseline    = picked_qsearch(position, state);
    const auto context       = State::make_context(position);
    boost_continuation_hint(position, context, state, quiet_evasion);

    EXPECT_EQ(picked_main_search(position, state, ply), main_baseline);
    EXPECT_EQ(picked_qsearch(position, state), q_baseline);
}

TEST_F(PickerTest, SkipNoncheckingQuietsKeepsQuietChecksAndWeakCaptures) {
    Board      position{WEAK_CAPTURE_FEN};
    const Move nonchecking{D1, D2};
    const Move checking{D1, G4};
    const Move weak_capture{D1, D7};
    ASSERT_TRUE(position.is_pseudo_legal(nonchecking));
    ASSERT_TRUE(position.is_pseudo_legal(checking));
    ASSERT_FALSE(position.gives_check(nonchecking));
    ASSERT_TRUE(position.gives_check(checking));

    killers.update(checking, ply);
    killers.update(nonchecking, ply);
    const auto context = State::make_context(position);
    auto       picker  = Picker::for_main_search(position, state, context, ply);
    picker.skip_nonchecking_quiets();
    const auto moves = collect_moves(picker);

    EXPECT_EQ(std::find(moves.begin(), moves.end(), nonchecking), moves.end());
    EXPECT_NE(std::find(moves.begin(), moves.end(), checking), moves.end());
    EXPECT_NE(std::find(moves.begin(), moves.end(), weak_capture), moves.end());
    for (const Move move : moves) {
        if (move.type() != MOVE_PROM && !position.is_capture(move))
            EXPECT_TRUE(position.gives_check(move)) << move.str();
    }
}

TEST_F(PickerTest, SkipNoncheckingQuietsIsNoOpForQSearchAndEvasions) {
    {
        SCOPED_TRACE("qsearch");
        auto picker = Picker::for_quiescence(board, state);
        picker.skip_nonchecking_quiets();
        EXPECT_EQ(collect_moves(picker), picked_qsearch(board, state));
    }
    {
        SCOPED_TRACE("in-check main search");
        Board      position{board_test::fen::one_legal_evasion};
        const auto context = State::make_context(position);
        auto       picker  = Picker::for_main_search(position, state, context, ply);
        picker.skip_nonchecking_quiets();
        EXPECT_EQ(sorted_move_bits(collect_moves(picker)),
                  sorted_move_bits(movegen::generate_evasions(position)));
    }
}

TEST_F(PickerTest, CaptureHistoryBreaksEqualVictimTiesAndPreservesTtPriority) {
    const Board position{"7k/8/8/2p1p3/8/3N4/6P1/4K3 w - - 0 1"};
    const Move  preferred{D3, E5};
    const Move  other{D3, C5};
    state.reward_capture(position, preferred, 4);
    state.penalize_capture(position, other, 4);
    EXPECT_EQ(picked_main_search(position, state, ply).front(), preferred);
    EXPECT_EQ(picked_qsearch(position, state).front(), preferred);
    expect_hash_move_first_once(picked_main_search(position, state, ply, other), other);
    expect_hash_move_first_once(picked_qsearch(position, state, other), other);
}

TEST_F(PickerTest, CaptureHistoryUsesSeePricesAcrossVictims) {
    const Board position{"7k/8/8/2p1b3/8/3N4/6P1/4K3 w - - 0 1"};
    const Move  higher_victim{D3, E5};
    const Move  lower_victim{D3, C5};
    EXPECT_EQ(picked_main_search(position, state, ply).front(), higher_victim);
    for (int i = 0; i < 6; ++i)
        state.reward_capture(position, lower_victim, 8);
    const int scaled_capture_history =
        state.capture_score(position, lower_victim) / capture_scoring::HistoryScoreDivisor;
    // This outweighs the SEE bishop/pawn gap, but not the evaluation-price gap.
    ASSERT_GT(scaled_capture_history, 285);
    ASSERT_LT(scaled_capture_history, 515);
    EXPECT_EQ(picked_main_search(position, state, ply).front(), lower_victim);
    EXPECT_EQ(picked_qsearch(position, state).front(), lower_victim);
}

TEST_F(PickerTest, CaptureOrderingKeepsScoringTimeHistorySnapshot) {
    const Board position{"7k/8/8/2p1p3/8/3N4/5pP1/K7 w - - 0 1"};
    const auto  frozen = picked_main_search(position, state, ply);
    ASSERT_GE(frozen.size(), 3U);
    for (int i = 0; i < 3; ++i)
        ASSERT_TRUE(position.is_capture(frozen[i]));
    auto picker = Picker::for_main_search(position, state, State::make_context(position), ply);
    ASSERT_EQ(picker.next(), frozen.front());
    for (int i = 0; i < 128; ++i) {
        state.penalize_capture(position, frozen[1], Limits::max_depth);
        state.reward_capture(position, frozen[2], Limits::max_depth);
    }
    EXPECT_EQ(collect_moves(picker), std::vector<Move>(frozen.begin() + 1, frozen.end()));
    EXPECT_EQ(picked_main_search(position, state, ply).front(), frozen[2]);
}

TEST_F(PickerTest, MainSeeUsesCachedHistoryRatherThanMutatedTable) {
    const Board position{"3r4/7k/q7/3r4/3QP3/8/R7/1K6 w - - 0 1"};
    const Move  first{A2, A6}, second{D4, D5};
    auto        picker =
        Picker::for_main_search(position, state, State::make_context(position), ply, NULL_MOVE);
    ASSERT_EQ(picker.next(), first);
    for (int i = 0; i < 128; ++i)
        state.penalize_capture(position, second, Limits::max_depth);
    // Victim/attacker scoring may first select the pawn taking the same rook.
    // The cached queen capture must still precede every quiet move.
    for (;;) {
        const Move move = picker.next();
        ASSERT_FALSE(move.is_null());
        ASSERT_TRUE(position.is_capture(move));
        if (move == second)
            break;
    }
    const auto fresh   = picked_main_search(position, state, ply);
    const auto capture = std::find(fresh.begin(), fresh.end(), second);
    const auto quiet =
        std::find_if(fresh.begin(), fresh.end(), [&](Move m) { return !position.is_capture(m); });
    ASSERT_NE(capture, fresh.end());
    ASSERT_NE(quiet, fresh.end());
    EXPECT_LT(quiet, capture);
}

TEST_F(PickerTest, MainCaptureStagingUsesHistoryWhileQuiescenceUsesZeroSee) {
    for (const auto fen :
         {"7k/8/5p2/4n3/3B4/8/8/K7 w - - 0 1", "7k/8/5p2/4b3/3B4/8/8/K7 w - - 0 1"}) {
        SCOPED_TRACE(fen);
        const Board position{fen};
        const Move  capture{D4, E5};
        const int   exchange = board_test::exchange_oracle(position, capture, see::piece_values);
        ASSERT_EQ(exchange, position.piece_type_on(E5) == KNIGHT ? -35 : 0);
        for (int history_sign : {-1, 0, 1}) {
            SCOPED_TRACE(history_sign);
            state.clear();
            for (int i = 0; i < 128; ++i) {
                if (history_sign < 0)
                    state.penalize_capture(position, capture, Limits::max_depth);
                if (history_sign > 0)
                    state.reward_capture(position, capture, Limits::max_depth);
            }
            const auto main_moves = picked_main_search(position, state, ply);
            const auto ci         = std::find(main_moves.begin(), main_moves.end(), capture);
            const auto qi         = std::find_if(main_moves.begin(), main_moves.end(), [&](Move m) {
                return !position.is_capture(m);
            });
            ASSERT_NE(ci, main_moves.end());
            ASSERT_NE(qi, main_moves.end());
            EXPECT_EQ(ci < qi, history_sign >= 0);
            const auto qsearch_moves = picked_qsearch(position, state);
            EXPECT_EQ(std::count(qsearch_moves.begin(), qsearch_moves.end(), capture),
                      exchange >= 0 ? 1 : 0);
            expect_hash_move_first_once(picked_main_search(position, state, ply, capture), capture);
            expect_hash_move_first_once(picked_qsearch(position, state, capture), capture);
        }
    }
}

TEST_F(PickerTest, VictimAttackerScoringOrdersAcrossVictimsAndKeepsTtPriority) {
    const Board position{"7k/8/8/b1n5/Q7/3N4/8/7K w - - 0 1"};
    const Move  knight{D3, C5}, queen{A4, A5};
    const Move  preferred = knight;
    EXPECT_EQ(picked_main_search(position, state, ply).front(), preferred);
    EXPECT_EQ(picked_qsearch(position, state).front(), preferred);
    expect_hash_move_first_once(picked_main_search(position, state, ply, queen), queen);
}

TEST_F(PickerTest, FlatDeferredCapturesFollowQuietMoves) {
    const Board position{"3r1r2/7k/3r4/8/3Q1b2/8/8/K7 w - - 0 1"};
    const Move  rook{D4, D6}, bishop{D4, F4};
    auto        moves = picked_main_search(position, state, ply);
    auto        ri    = std::find(moves.begin(), moves.end(), rook);
    auto        bi    = std::find(moves.begin(), moves.end(), bishop);
    ASSERT_NE(ri, moves.end());
    ASSERT_NE(bi, moves.end());
    const auto first = std::min(ri, bi);
    for (auto it = first; it != moves.end(); ++it)
        EXPECT_TRUE(position.is_capture(*it));
}

TEST(CaptureScoringTest, KeepsScoresWithinStageAndRecoversHistory) {
    for (int victim_value : {100, 350, 385, 655, 1220})
        for (int attacker_value : {0, 100, 350, 385, 655, 1220})
            for (int scaled_capture_history : {-1024, -1, 0, 1, 1024}) {
                const int order_score =
                    capture_scoring::score(victim_value, attacker_value, scaled_capture_history);
                EXPECT_GT(order_score, 0);
                EXPECT_LT(order_score, 1 << 22);
                EXPECT_EQ(capture_scoring::scaled_history_from_score(
                              order_score, victim_value, attacker_value),
                          scaled_capture_history);
            }
}

TEST(CaptureScoringTest, MainSearchSeeThresholdPreservesSignedTruncation) {
    struct Case {
        PieceType victim;
        int       scaled_capture_history;
        int       expected_threshold;
    };
    constexpr std::array cases{
        Case{PAWN, 0, -50},
        Case{PAWN, -1, -49},
        Case{PAWN, 1, -50},
        Case{PAWN, -2, -49},
        Case{PAWN, 2, -51},
        Case{PAWN, -101, 0},
        Case{PAWN, -102, 1},
        Case{KNIGHT, 0, -175},
        Case{BISHOP, 0, -192},
        Case{ROOK, 0, -327},
        Case{QUEEN, 0, -610},
    };
    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.victim);
        SCOPED_TRACE(tc.scaled_capture_history);
        EXPECT_EQ(capture_scoring::main_search_see_threshold(tc.victim, tc.scaled_capture_history),
                  tc.expected_threshold);
    }
}

} // namespace search::ordering
