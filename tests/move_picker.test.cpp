#include "move_picker.hpp"

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "history.hpp"
#include "killers.hpp"
#include "movegen.hpp"
#include "test_util.hpp"

class MovePickerTest : public ::testing::Test {
protected:
    int          ply = 5;
    TestBoard    board{POS3};
    KillerMoves  killers;
    QuietHistory quiet_history;
};

namespace {
constexpr std::string_view PROMOTION_FEN             = "4k3/P6p/8/8/8/8/p6P/4K3 w - - 0 1";
constexpr std::string_view CAPTURE_PROMOTION_FEN     = "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1";
constexpr std::string_view CASTLE_FEN                = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
constexpr std::string_view QUIET_EVASION_FEN         = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
constexpr std::string_view WEAK_CAPTURE_FEN          = "2b3k1/3p4/8/8/8/8/8/3Q2K1 w - - 0 1";
constexpr std::string_view PROMOTION_AND_CAPTURE_FEN = "4k3/P7/8/1p6/3N4/8/8/4K3 w - - 0 1";

std::vector<uint16_t> sorted_move_bits(MoveList movelist) {
    std::vector<uint16_t> bits;
    for (const Move& move : movelist)
        bits.push_back(move.bits);
    std::sort(bits.begin(), bits.end());
    return bits;
}

std::vector<uint16_t> sorted_move_bits(const std::vector<Move>& moves) {
    std::vector<uint16_t> bits;
    for (const Move& move : moves)
        bits.push_back(move.bits);
    std::sort(bits.begin(), bits.end());
    return bits;
}

std::vector<Move> collect_moves(MovePicker& picker) {
    std::vector<Move> moves;
    moves.reserve(MAX_MOVES);
    for (Move move = picker.next(); !move.is_null(); move = picker.next())
        moves.push_back(move);
    return moves;
}

std::vector<Move> picked_main_search(Board&        board,
                                     KillerMoves&  killers,
                                     QuietHistory& quiet_history,
                                     int           ply,
                                     Move          tt_move = NULL_MOVE) {
    MovePicker picker = MovePicker::main_search(board, killers, quiet_history, ply, tt_move);
    return collect_moves(picker);
}

std::vector<Move>
picked_qsearch(Board& board, QuietHistory& quiet_history, Move tt_move = NULL_MOVE) {
    MovePicker picker = MovePicker::qsearch(board, quiet_history, tt_move);
    return collect_moves(picker);
}

void expect_hash_move_first_once(const std::vector<Move>& moves, Move expected) {
    ASSERT_FALSE(moves.empty());
    EXPECT_EQ(moves.front(), expected);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), expected), 1);
}

std::vector<Move> picked_root_order(Board&        board,
                                    KillerMoves&  killers,
                                    QuietHistory& quiet_history,
                                    int           ply,
                                    Move          tt_move = NULL_MOVE) {
    return picked_main_search(board, killers, quiet_history, ply, tt_move);
}

bool expected_good_noisy(const Board& board, Move move) {
    return move.type() == MOVE_PROM || board.seeMove(move) >= 0;
}
} // namespace

TEST_F(MovePickerTest, MainSearchPicksGeneratedMoves) {
    const auto moves = picked_main_search(board, killers, quiet_history, ply);

    EXPECT_FALSE(moves.empty());
}

TEST_F(MovePickerTest, MainSearchKeepsHistoryBelowKillerPriority) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    for (int i = 0; i < 8; ++i)
        quiet_history.reward(
            board.side_to_move(), hist_move.from(), hist_move.to(), MAX_SEARCH_DEPTH);

    const auto moves     = picked_main_search(board, killers, quiet_history, ply);
    const auto killer_it = std::find(moves.begin(), moves.end(), killer_move);
    const auto hist_it   = std::find(moves.begin(), moves.end(), hist_move);

    ASSERT_NE(killer_it, moves.end());
    ASSERT_NE(hist_it, moves.end());
    EXPECT_LT(killer_it, hist_it);
    EXPECT_LE(quiet_history.get(board.side_to_move(), hist_move.from(), hist_move.to()),
              QuietHistory::MAX_SCORE);
}

TEST_F(MovePickerTest, MainSearchOrdersSaturatedHistoryAboveLightHistoryBelowKiller) {
    Move killer_move    = Move(A5, A4);
    Move saturated_move = Move(A5, A6);
    Move light_move     = Move(E2, E3);

    killers.update(killer_move, ply);
    quiet_history.reward(board.side_to_move(), light_move.from(), light_move.to(), 1);
    for (int i = 0; i < 8; ++i)
        quiet_history.reward(
            board.side_to_move(), saturated_move.from(), saturated_move.to(), MAX_SEARCH_DEPTH);

    const auto moves        = picked_main_search(board, killers, quiet_history, ply);
    const auto killer_it    = std::find(moves.begin(), moves.end(), killer_move);
    const auto saturated_it = std::find(moves.begin(), moves.end(), saturated_move);
    const auto light_it     = std::find(moves.begin(), moves.end(), light_move);

    ASSERT_NE(killer_it, moves.end());
    ASSERT_NE(saturated_it, moves.end());
    ASSERT_NE(light_it, moves.end());
    EXPECT_LT(killer_it, saturated_it);
    EXPECT_LT(saturated_it, light_it);
    EXPECT_LE(quiet_history.get(board.side_to_move(), saturated_move.from(), saturated_move.to()),
              QuietHistory::MAX_SCORE);
    EXPECT_GT(quiet_history.get(board.side_to_move(), saturated_move.from(), saturated_move.to()),
              quiet_history.get(board.side_to_move(), light_move.from(), light_move.to()));
}

TEST_F(MovePickerTest, MainSearchKeepsCaptureKillerAndHistoryOrder) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    quiet_history.reward(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves = picked_main_search(board, killers, quiet_history, ply);

    ASSERT_GT(moves.size(), 3U);
    EXPECT_EQ(moves[0], Move(B4, F4));
    EXPECT_EQ(moves[1], killer_move);
    EXPECT_EQ(moves[2], hist_move);
}

TEST_F(MovePickerTest, QSearchNotInCheckGeneratesNoisyMovesOnly) {
    ASSERT_FALSE(board.is_check());

    const auto moves = picked_qsearch(board, quiet_history);

    MoveList expected;
    for (const Move& move : movegen::generate_noisy(board)) {
        if (expected_good_noisy(board, move))
            expected.add(move);
    }
    EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(expected));
    for (const Move& move : moves) {
        EXPECT_TRUE(move.type() == MOVE_PROM || board.is_capture(move)) << move.str();
        EXPECT_TRUE(expected_good_noisy(board, move)) << move.str();
    }
}

TEST_F(MovePickerTest, QSearchNotInCheckDoesNotReturnWeakCaptures) {
    TestBoard weak_capture_board{std::string(WEAK_CAPTURE_FEN)};

    const auto moves = picked_qsearch(weak_capture_board, quiet_history);

    EXPECT_EQ(std::find(moves.begin(), moves.end(), Move(D1, D7)), moves.end());
}

TEST_F(MovePickerTest, QSearchReturnsHashCaptureFirstOutsideCheck) {
    Move tt_capture = Move(B4, F4);
    ASSERT_FALSE(board.is_check());
    ASSERT_TRUE(board.is_capture(tt_capture));

    const auto moves = picked_qsearch(board, quiet_history, tt_capture);

    expect_hash_move_first_once(moves, tt_capture);
}

TEST_F(MovePickerTest, QSearchReturnsHashPromotionFirstOutsideCheck) {
    TestBoard promotion_board{std::string(PROMOTION_FEN)};
    Move      tt_promotion = Move(A7, A8, MOVE_PROM, QUEEN);
    ASSERT_FALSE(promotion_board.is_check());
    ASSERT_TRUE(promotion_board.is_pseudo_legal(tt_promotion));

    const auto moves = picked_qsearch(promotion_board, quiet_history, tt_promotion);

    expect_hash_move_first_once(moves, tt_promotion);
}

TEST_F(MovePickerTest, QSearchIgnoresQuietHashMoveWhenNotInCheck) {
    Move quiet_tt_move = Move(E2, E3);
    ASSERT_FALSE(board.is_check());
    ASSERT_TRUE(board.is_pseudo_legal(quiet_tt_move));
    ASSERT_FALSE(board.is_capture(quiet_tt_move));

    const auto moves = picked_qsearch(board, quiet_history, quiet_tt_move);

    ASSERT_FALSE(moves.empty());
    EXPECT_NE(moves.front(), quiet_tt_move);
    EXPECT_EQ(std::find(moves.begin(), moves.end(), quiet_tt_move), moves.end());
}

TEST_F(MovePickerTest, QSearchInCheckGeneratesEvasionsIncludingQuietEvasions) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    ASSERT_TRUE(evasion_board.is_check());

    const auto moves = picked_qsearch(evasion_board, quiet_history);

    EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(movegen::generate_evasions(evasion_board)));
    EXPECT_NE(std::find(moves.begin(), moves.end(), Move(A8, B8)), moves.end());
}

TEST_F(MovePickerTest, QSearchReturnsQuietHashEvasionFirstWhileInCheck) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    Move      quiet_evasion = Move(A8, B8);
    ASSERT_TRUE(evasion_board.is_check());
    ASSERT_TRUE(evasion_board.is_legal_pseudo_move(quiet_evasion));

    const auto moves = picked_qsearch(evasion_board, quiet_history, quiet_evasion);

    expect_hash_move_first_once(moves, quiet_evasion);
}

TEST_F(MovePickerTest, MainSearchKeepsSameMoveSetForEqualPriorityMoves) {
    TestBoard quiet_board{STARTFEN};

    MoveList   generated    = movegen::generate_pseudo_legal(quiet_board);
    const auto picked_order = picked_main_search(quiet_board, killers, quiet_history, ply);

    EXPECT_EQ(sorted_move_bits(picked_order), sorted_move_bits(generated));
}

TEST_F(MovePickerTest, MainSearchIsDeterministicForEqualPriorityMoves) {
    TestBoard quiet_board{STARTFEN};

    const auto first_order  = picked_main_search(quiet_board, killers, quiet_history, ply);
    const auto second_order = picked_main_search(quiet_board, killers, quiet_history, ply);

    ASSERT_GT(first_order.size(), 1U);
    EXPECT_EQ(second_order, first_order);
}

TEST_F(MovePickerTest, MainSearchKeepsHashMoveFirst) {
    TestBoard quiet_board{STARTFEN};
    Move      tt_move = Move(E2, E4);

    const auto moves = picked_main_search(quiet_board, killers, quiet_history, ply, tt_move);

    ASSERT_FALSE(moves.empty());
    EXPECT_EQ(moves[0], tt_move);
}

TEST_F(MovePickerTest, MainSearchKeepsHashCaptureKillerAndHistoryOrder) {
    Move tt_move     = Move(E2, E3);
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    quiet_history.reward(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves = picked_main_search(board, killers, quiet_history, ply, tt_move);

    ASSERT_GT(moves.size(), 3U);
    EXPECT_EQ(moves[0], tt_move);
    EXPECT_EQ(moves[1], Move(B4, F4));
    EXPECT_EQ(moves[2], killer_move);
    EXPECT_EQ(moves[3], hist_move);
}

TEST_F(MovePickerTest, MainSearchKeepsHashFirstAndDeterministicEqualPriorityOrder) {
    TestBoard quiet_board{STARTFEN};
    Move      tt_move = Move(E2, E4);

    const auto first_order  = picked_main_search(quiet_board, killers, quiet_history, ply, tt_move);
    const auto second_order = picked_main_search(quiet_board, killers, quiet_history, ply, tt_move);

    ASSERT_GT(first_order.size(), 2U);
    EXPECT_EQ(first_order.front(), tt_move);
    EXPECT_EQ(std::count(first_order.begin(), first_order.end(), tt_move), 1);
    EXPECT_EQ(second_order, first_order);
}

TEST_F(MovePickerTest, MainSearchPicksSamePseudoLegalMoveSet) {
    for (std::string_view fen : {std::string_view{STARTFEN},
                                 std::string_view{POS2},
                                 std::string_view{POS3},
                                 std::string_view{ENPASSANT_A3},
                                 PROMOTION_FEN,
                                 CAPTURE_PROMOTION_FEN,
                                 CASTLE_FEN}) {
        TestBoard board{std::string(fen)};
        ASSERT_FALSE(board.is_check()) << fen;

        const auto picked = picked_root_order(board, killers, quiet_history, ply);
        MoveList   picked_list;
        for (Move move : picked)
            picked_list.add(move);

        EXPECT_EQ(sorted_move_bits(picked_list),
                  sorted_move_bits(movegen::generate_pseudo_legal(board)))
            << fen;
    }
}

TEST_F(MovePickerTest, MainSearchInCheckGeneratesEvasionsThroughMainSearchMode) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    ASSERT_TRUE(evasion_board.is_check());

    const auto moves = picked_main_search(evasion_board, killers, quiet_history, ply);

    EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(movegen::generate_evasions(evasion_board)));
    EXPECT_NE(std::find(moves.begin(), moves.end(), Move(A8, B8)), moves.end());
}

TEST_F(MovePickerTest, MainSearchInCheckDoesNotReturnNonEvasionHintsAsHints) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    Move      non_evasion_king_move = Move(A8, A7);
    ASSERT_TRUE(evasion_board.is_check());
    ASSERT_TRUE(evasion_board.is_pseudo_legal(non_evasion_king_move));
    ASSERT_FALSE(evasion_board.is_legal_pseudo_move(non_evasion_king_move));

    const auto hinted =
        picked_main_search(evasion_board, killers, quiet_history, ply, non_evasion_king_move);
    const auto baseline = picked_main_search(evasion_board, killers, quiet_history, ply);

    EXPECT_EQ(hinted, baseline);
}

TEST_F(MovePickerTest, MainSearchKeepsHashFirstWithoutDuplicates) {
    Move tt_move = Move(E2, E3);

    const auto moves = picked_root_order(board, killers, quiet_history, ply, tt_move);

    ASSERT_FALSE(moves.empty());
    EXPECT_EQ(moves[0], tt_move);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), tt_move), 1);
}

TEST_F(MovePickerTest, MainSearchKeepsCaptureKillerAndHistoryPriorityBands) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    quiet_history.reward(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves = picked_root_order(board, killers, quiet_history, ply);

    ASSERT_GT(moves.size(), 4U);
    EXPECT_EQ(moves[0], Move(B4, F4));
    EXPECT_EQ(moves[1], killer_move);
    EXPECT_EQ(moves[2], hist_move);
}

TEST_F(MovePickerTest, MainSearchSuppressesKillerDuplicates) {
    TestBoard quiet_board{STARTFEN};
    Move      killer_1 = Move(E2, E4);
    Move      killer_2 = Move(D2, D4);

    killers.update(killer_2, ply);
    killers.update(killer_1, ply);

    const auto moves = picked_root_order(quiet_board, killers, quiet_history, ply);

    ASSERT_GT(moves.size(), 2U);
    EXPECT_EQ(moves[0], killer_1);
    EXPECT_EQ(moves[1], killer_2);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), killer_1), 1);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), killer_2), 1);
}

TEST_F(MovePickerTest, MainSearchKeepsWeakTacticalsAfterQuiets) {
    TestBoard board{std::string(WEAK_CAPTURE_FEN)};

    const auto moves    = picked_root_order(board, killers, quiet_history, ply);
    const auto weak_it  = std::find(moves.begin(), moves.end(), Move(D1, D7));
    const auto quiet_it = std::find_if(moves.begin(), moves.end(), [&](Move move) {
        return move.type() != MOVE_PROM && !board.is_capture(move);
    });

    ASSERT_NE(weak_it, moves.end());
    ASSERT_NE(quiet_it, moves.end());
    EXPECT_LT(quiet_it, weak_it);
}

TEST_F(MovePickerTest, MainSearchKeepsPromotionsAboveOrdinaryCaptures) {
    TestBoard board{std::string(PROMOTION_AND_CAPTURE_FEN)};
    Move      promotion = Move(A7, A8, MOVE_PROM, QUEEN);
    Move      capture   = Move(D4, B5);

    const auto moves        = picked_root_order(board, killers, quiet_history, ply);
    const auto promotion_it = std::find(moves.begin(), moves.end(), promotion);
    const auto capture_it   = std::find(moves.begin(), moves.end(), capture);
    ASSERT_NE(promotion_it, moves.end());
    ASSERT_NE(capture_it, moves.end());
    EXPECT_LT(promotion_it, capture_it);
}

TEST_F(MovePickerTest, MainSearchCoversSpecialMoveStages) {
    TestBoard promotion_board{std::string(PROMOTION_FEN)};
    TestBoard capture_promotion_board{std::string(CAPTURE_PROMOTION_FEN)};
    TestBoard castle_board{std::string(CASTLE_FEN)};
    TestBoard ep_board{ENPASSANT_A3};

    const auto promotions = picked_root_order(promotion_board, killers, quiet_history, ply);
    const auto capture_promotions =
        picked_root_order(capture_promotion_board, killers, quiet_history, ply);
    const auto castles = picked_root_order(castle_board, killers, quiet_history, ply);
    const auto eps     = picked_root_order(ep_board, killers, quiet_history, ply);

    EXPECT_NE(std::find(promotions.begin(), promotions.end(), Move(A7, A8, MOVE_PROM, QUEEN)),
              promotions.end());
    EXPECT_NE(std::find(capture_promotions.begin(),
                        capture_promotions.end(),
                        Move(A7, B8, MOVE_PROM, QUEEN)),
              capture_promotions.end());
    EXPECT_NE(std::find(castles.begin(), castles.end(), Move(E1, G1, MOVE_CASTLE)), castles.end());
    EXPECT_NE(std::find(castles.begin(), castles.end(), Move(E1, C1, MOVE_CASTLE)), castles.end());
    EXPECT_NE(std::find(eps.begin(), eps.end(), Move(B4, A3, MOVE_EP)), eps.end());
}

TEST_F(MovePickerTest, MainSearchKeepsDeterministicEqualPriorityQuietOrder) {
    TestBoard quiet_board{STARTFEN};

    const auto first_order  = picked_root_order(quiet_board, killers, quiet_history, ply);
    const auto second_order = picked_root_order(quiet_board, killers, quiet_history, ply);

    ASSERT_GT(first_order.size(), 1U);
    EXPECT_EQ(second_order, first_order);
}

TEST_F(MovePickerTest, SkipQuietMovesSkipsKillersAndQuietsButKeepsBadNoisyMoves) {
    TestBoard weak_capture_board{std::string(WEAK_CAPTURE_FEN)};
    Move      killer_move = Move(D1, D2);
    ASSERT_TRUE(weak_capture_board.is_pseudo_legal(killer_move));
    ASSERT_FALSE(weak_capture_board.is_capture(killer_move));
    killers.update(killer_move, ply);

    MovePicker picker =
        MovePicker::main_search(weak_capture_board, killers, quiet_history, ply, NULL_MOVE);
    picker.skip_quiet_moves();
    const auto moves = collect_moves(picker);

    EXPECT_EQ(std::find(moves.begin(), moves.end(), killer_move), moves.end());
    EXPECT_NE(std::find(moves.begin(), moves.end(), Move(D1, D7)), moves.end());
    for (Move move : moves) {
        EXPECT_TRUE(move.type() == MOVE_PROM || weak_capture_board.is_capture(move)) << move.str();
    }
}

TEST_F(MovePickerTest, SkipQuietMovesIsNoOpForQSearch) {
    MovePicker picker = MovePicker::qsearch(board, quiet_history, NULL_MOVE);
    picker.skip_quiet_moves();

    EXPECT_EQ(collect_moves(picker), picked_qsearch(board, quiet_history));
}

TEST_F(MovePickerTest, SkipQuietMovesIsNoOpForInCheckMainSearch) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    ASSERT_TRUE(evasion_board.is_check());

    MovePicker picker =
        MovePicker::main_search(evasion_board, killers, quiet_history, ply, NULL_MOVE);
    picker.skip_quiet_moves();

    EXPECT_EQ(sorted_move_bits(collect_moves(picker)),
              sorted_move_bits(movegen::generate_evasions(evasion_board)));
}
