#include "move_picker.hpp"

#include <algorithm>
#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include "eval.hpp"
#include "history.hpp"
#include "killers.hpp"
#include "movegen.hpp"
#include "test_util.hpp"

class MovePickerTest : public ::testing::Test {
protected:
    int          ply = 5;
    TestBoard    board{POS3};
    KillerMoves  killers;
    HistoryTable history;
};

namespace {
constexpr std::string_view PROMOTION_FEN                  = "4k3/P6p/8/8/8/8/p6P/4K3 w - - 0 1";
constexpr std::string_view CAPTURE_PROMOTION_FEN          = "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1";
constexpr std::string_view CASTLE_FEN                     = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
constexpr std::string_view QUIET_EVASION_FEN              = "k7/8/2K5/8/8/8/R7/8 b - - 0 1";
constexpr std::string_view WEAK_CAPTURE_FEN               = "2b3k1/3p4/8/8/8/8/8/3Q2K1 w - - 0 1";
constexpr MoveScore        EXPECTED_CAPTURE_VICTIM_WEIGHT = 7;

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

std::vector<Move> picked_moves(Board&         board,
                               KillerMoves&   killers,
                               HistoryTable&  history,
                               int            ply,
                               MovePickerMode mode,
                               Move           pv_move = NULL_MOVE,
                               Move           tt_move = NULL_MOVE) {
    std::vector<Move> moves;
    MovePicker        picker{{board, killers, history, ply, pv_move, tt_move}, mode};

    for (Move move = picker.next(); !move.is_null(); move = picker.next())
        moves.push_back(move);

    return moves;
}

std::vector<std::pair<Move, MoveScore>> picked_moves_with_scores(Board&         board,
                                                                 KillerMoves&   killers,
                                                                 HistoryTable&  history,
                                                                 int            ply,
                                                                 MovePickerMode mode,
                                                                 Move           pv_move = NULL_MOVE,
                                                                 Move tt_move = NULL_MOVE) {
    std::vector<std::pair<Move, MoveScore>> moves;
    MovePicker picker{{board, killers, history, ply, pv_move, tt_move}, mode};

    for (Move move = picker.next(); !move.is_null(); move = picker.next())
        moves.push_back({move, picker.last_score()});

    return moves;
}

std::vector<std::pair<Move, MovePickSource>> picked_moves_with_sources(Board&         board,
                                                                       KillerMoves&   killers,
                                                                       HistoryTable&  history,
                                                                       int            ply,
                                                                       MovePickerMode mode,
                                                                       Move pv_move = NULL_MOVE,
                                                                       Move tt_move = NULL_MOVE) {
    std::vector<std::pair<Move, MovePickSource>> moves;
    MovePicker picker{{board, killers, history, ply, pv_move, tt_move}, mode};

    for (Move move = picker.next(); !move.is_null(); move = picker.next())
        moves.push_back({move, picker.last_source()});

    return moves;
}

void expect_hash_move_first_once(const std::vector<std::pair<Move, MovePickSource>>& moves,
                                 Move                                                expected) {
    ASSERT_FALSE(moves.empty());
    EXPECT_EQ(moves.front(), std::make_pair(expected, MovePickSource::Hash));
    EXPECT_EQ(std::count_if(moves.begin(),
                            moves.end(),
                            [&](const auto& entry) { return entry.first == expected; }),
              1);
}

std::vector<std::pair<Move, MoveScore>> staged_picked_moves_with_scores(Board&        board,
                                                                        KillerMoves&  killers,
                                                                        HistoryTable& history,
                                                                        int           ply,
                                                                        Move pv_move = NULL_MOVE,
                                                                        Move tt_move = NULL_MOVE) {
    std::vector<std::pair<Move, MoveScore>> moves;
    MovePicker picker{{board, killers, history, ply, pv_move, tt_move}, MovePickerMode::MainSearch};

    for (Move move = picker.next(); !move.is_null(); move = picker.next())
        moves.push_back({move, picker.last_score()});

    return moves;
}

std::vector<std::pair<Move, MovePickSource>>
staged_picked_moves_with_sources(Board&        board,
                                 KillerMoves&  killers,
                                 HistoryTable& history,
                                 int           ply,
                                 Move          pv_move = NULL_MOVE,
                                 Move          tt_move = NULL_MOVE) {
    return picked_moves_with_sources(
        board, killers, history, ply, MovePickerMode::MainSearch, pv_move, tt_move);
}

std::vector<Move> staged_picked_moves(Board&        board,
                                      KillerMoves&  killers,
                                      HistoryTable& history,
                                      int           ply,
                                      Move          pv_move = NULL_MOVE,
                                      Move          tt_move = NULL_MOVE) {
    std::vector<Move> moves;
    for (const auto& [move, score] :
         staged_picked_moves_with_scores(board, killers, history, ply, pv_move, tt_move)) {
        (void)score;
        moves.push_back(move);
    }
    return moves;
}

MoveScore priority_band(MoveScore score) {
    if (score >= PRIORITY_PV)
        return PRIORITY_PV;
    if (score >= PRIORITY_HASH)
        return PRIORITY_HASH;
    if (score >= PRIORITY_PROM)
        return PRIORITY_PROM;
    if (score >= PRIORITY_CAPTURE)
        return PRIORITY_CAPTURE;
    if (score >= PRIORITY_KILLER)
        return PRIORITY_KILLER;
    if (score > PRIORITY_WEAK)
        return PRIORITY_HISTORY;
    return PRIORITY_WEAK;
}

MoveScore expected_capture_score(const Board& board, Move move) {
    const int see_score = board.seeMove(move);
    if (see_score < 0)
        return PRIORITY_WEAK;

    return PRIORITY_CAPTURE +
           EXPECTED_CAPTURE_VICTIM_WEIGHT * eval::piece(board.captured_piece_type(move)).mg +
           see_score;
}

std::vector<MoveScore> picked_priority_bands(const std::vector<std::pair<Move, MoveScore>>& moves) {
    std::vector<MoveScore> bands;
    for (const auto& [move, score] : moves) {
        (void)move;
        bands.push_back(priority_band(score));
    }
    return bands;
}
} // namespace

TEST_F(MovePickerTest, MainSearchPicksGeneratedMoves) {
    const auto moves = picked_moves(board, killers, history, ply, MovePickerMode::MainSearch);

    EXPECT_FALSE(moves.empty());
}

TEST_F(MovePickerTest, MainSearchKeepsHistoryBelowKillerPriority) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    for (int i = 0; i < 8; ++i)
        history.update(board.side_to_move(), hist_move.from(), hist_move.to(), MAX_SEARCH_DEPTH);

    const auto moves     = picked_moves(board, killers, history, ply, MovePickerMode::MainSearch);
    const auto killer_it = std::find(moves.begin(), moves.end(), killer_move);
    const auto hist_it   = std::find(moves.begin(), moves.end(), hist_move);

    ASSERT_NE(killer_it, moves.end());
    ASSERT_NE(hist_it, moves.end());
    EXPECT_LT(killer_it, hist_it);
    EXPECT_LE(history.get(board.side_to_move(), hist_move.from(), hist_move.to()),
              PRIORITY_HISTORY);
}

TEST_F(MovePickerTest, MainSearchOrdersSaturatedHistoryAboveLightHistoryBelowKiller) {
    Move killer_move    = Move(A5, A4);
    Move saturated_move = Move(A5, A6);
    Move light_move     = Move(E2, E3);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), light_move.from(), light_move.to(), 1);
    for (int i = 0; i < 8; ++i)
        history.update(
            board.side_to_move(), saturated_move.from(), saturated_move.to(), MAX_SEARCH_DEPTH);

    const auto moves     = picked_moves(board, killers, history, ply, MovePickerMode::MainSearch);
    const auto killer_it = std::find(moves.begin(), moves.end(), killer_move);
    const auto saturated_it = std::find(moves.begin(), moves.end(), saturated_move);
    const auto light_it     = std::find(moves.begin(), moves.end(), light_move);

    ASSERT_NE(killer_it, moves.end());
    ASSERT_NE(saturated_it, moves.end());
    ASSERT_NE(light_it, moves.end());
    EXPECT_LT(killer_it, saturated_it);
    EXPECT_LT(saturated_it, light_it);
    EXPECT_LT(history.get(board.side_to_move(), saturated_move.from(), saturated_move.to()),
              PRIORITY_KILLER);
    EXPECT_GT(history.get(board.side_to_move(), saturated_move.from(), saturated_move.to()),
              history.get(board.side_to_move(), light_move.from(), light_move.to()));
}

TEST_F(MovePickerTest, MainSearchKeepsCaptureKillerAndHistoryOrder) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves = picked_moves(board, killers, history, ply, MovePickerMode::MainSearch);

    ASSERT_GT(moves.size(), 3U);
    EXPECT_EQ(moves[0], Move(B4, F4));
    EXPECT_EQ(moves[1], killer_move);
    EXPECT_EQ(moves[2], hist_move);
}

TEST_F(MovePickerTest, GoodCaptureScoreIncludesVictimBonusAndSee) {
    TestBoard capture_board{"1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - 0 1"};

    const auto moves =
        picked_moves_with_scores(capture_board, killers, history, ply, MovePickerMode::QSearch);
    const auto move_it = std::find_if(moves.begin(), moves.end(), [](const auto& move_and_score) {
        return move_and_score.first == Move(E1, E5);
    });

    ASSERT_NE(move_it, moves.end());
    EXPECT_EQ(move_it->second,
              PRIORITY_CAPTURE + EXPECTED_CAPTURE_VICTIM_WEIGHT * PAWN_MG +
                  capture_board.seeMove(Move(E1, E5)));
}

TEST_F(MovePickerTest, MainSearchWeakCaptureScoreRemainsWeakPriority) {
    TestBoard weak_capture_board{std::string(WEAK_CAPTURE_FEN)};

    const auto moves = picked_moves_with_scores(
        weak_capture_board, killers, history, ply, MovePickerMode::MainSearch);
    const auto move_it = std::find_if(moves.begin(), moves.end(), [](const auto& move_and_score) {
        return move_and_score.first == Move(D1, D7);
    });

    ASSERT_NE(move_it, moves.end());
    EXPECT_EQ(move_it->second, PRIORITY_WEAK);
}

TEST_F(MovePickerTest, EnPassantCaptureScoreUsesPawnVictim) {
    TestBoard ep_board{ENPASSANT_A3};

    const auto moves =
        picked_moves_with_scores(ep_board, killers, history, ply, MovePickerMode::QSearch);
    const auto move_it = std::find_if(moves.begin(), moves.end(), [](const auto& move_and_score) {
        return move_and_score.first == Move(B4, A3, MOVE_EP);
    });

    ASSERT_NE(move_it, moves.end());
    EXPECT_EQ(move_it->second,
              PRIORITY_CAPTURE + EXPECTED_CAPTURE_VICTIM_WEIGHT * PAWN_MG +
                  ep_board.seeMove(Move(B4, A3, MOVE_EP)));
}

TEST_F(MovePickerTest, QSearchNotInCheckGeneratesNoisyMovesOnly) {
    ASSERT_FALSE(board.is_check());

    const auto moves = picked_moves(board, killers, history, ply, MovePickerMode::QSearch);

    MoveList expected;
    for (const Move& move : movegen::generate_noisy(board)) {
        if (move.type() == MOVE_PROM || expected_capture_score(board, move) >= PRIORITY_CAPTURE)
            expected.add(move);
    }
    EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(expected));
    for (const Move& move : moves) {
        EXPECT_TRUE(move.type() == MOVE_PROM || board.is_capture(move)) << move.str();
        EXPECT_TRUE(move.type() == MOVE_PROM ||
                    expected_capture_score(board, move) >= PRIORITY_CAPTURE)
            << move.str();
    }
}

TEST_F(MovePickerTest, QSearchNotInCheckDoesNotReturnWeakCaptures) {
    TestBoard weak_capture_board{std::string(WEAK_CAPTURE_FEN)};

    const auto moves =
        picked_moves(weak_capture_board, killers, history, ply, MovePickerMode::QSearch);

    EXPECT_EQ(std::find(moves.begin(), moves.end(), Move(D1, D7)), moves.end());
}

TEST_F(MovePickerTest, QSearchReturnsHashCaptureFirstOutsideCheck) {
    Move tt_capture = Move(B4, F4);
    ASSERT_FALSE(board.is_check());
    ASSERT_TRUE(board.is_capture(tt_capture));

    const auto moves = picked_moves_with_sources(
        board, killers, history, ply, MovePickerMode::QSearch, NULL_MOVE, tt_capture);

    expect_hash_move_first_once(moves, tt_capture);
}

TEST_F(MovePickerTest, QSearchReturnsHashPromotionFirstOutsideCheck) {
    TestBoard promotion_board{std::string(PROMOTION_FEN)};
    Move      tt_promotion = Move(A7, A8, MOVE_PROM, QUEEN);
    ASSERT_FALSE(promotion_board.is_check());
    ASSERT_TRUE(promotion_board.is_pseudo_legal(tt_promotion));

    const auto moves = picked_moves_with_sources(
        promotion_board, killers, history, ply, MovePickerMode::QSearch, NULL_MOVE, tt_promotion);

    expect_hash_move_first_once(moves, tt_promotion);
}

TEST_F(MovePickerTest, QSearchIgnoresQuietHashMoveOutsideCheck) {
    Move quiet_tt_move = Move(E2, E3);
    ASSERT_FALSE(board.is_check());
    ASSERT_TRUE(board.is_pseudo_legal(quiet_tt_move));
    ASSERT_FALSE(board.is_capture(quiet_tt_move));

    const auto moves = picked_moves_with_sources(
        board, killers, history, ply, MovePickerMode::QSearch, NULL_MOVE, quiet_tt_move);

    ASSERT_FALSE(moves.empty());
    EXPECT_NE(moves.front().first, quiet_tt_move);
    EXPECT_EQ(std::find_if(moves.begin(),
                           moves.end(),
                           [&](const auto& entry) { return entry.first == quiet_tt_move; }),
              moves.end());
}

TEST_F(MovePickerTest, QSearchInCheckGeneratesEvasionsIncludingQuietEvasions) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    ASSERT_TRUE(evasion_board.is_check());

    const auto moves = picked_moves(evasion_board, killers, history, ply, MovePickerMode::QSearch);

    EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(movegen::generate_evasions(evasion_board)));
    EXPECT_NE(std::find(moves.begin(), moves.end(), Move(A8, B8)), moves.end());
}

TEST_F(MovePickerTest, QSearchReturnsQuietHashEvasionFirstWhileInCheck) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    Move      quiet_evasion = Move(A8, B8);
    ASSERT_TRUE(evasion_board.is_check());
    ASSERT_TRUE(evasion_board.is_legal_pseudo_move(quiet_evasion));

    const auto moves = picked_moves_with_sources(
        evasion_board, killers, history, ply, MovePickerMode::QSearch, NULL_MOVE, quiet_evasion);

    expect_hash_move_first_once(moves, quiet_evasion);
}

TEST_F(MovePickerTest, QSearchInCheckDoesNotUseKillerStagesForQuietEvasions) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    Move      quiet_evasion = Move(A8, B8);
    killers.update(quiet_evasion, ply);
    ASSERT_TRUE(evasion_board.is_check());

    MovePicker picker{{evasion_board, killers, history, ply}, MovePickerMode::QSearch};

    MovePickSource quiet_evasion_source = MovePickSource::None;
    for (Move picked = picker.next(); !picked.is_null(); picked = picker.next()) {
        if (picked == quiet_evasion) {
            quiet_evasion_source = picker.last_source();
            break;
        }
    }

    EXPECT_EQ(quiet_evasion_source, MovePickSource::Evasion);
}

TEST_F(MovePickerTest, MainSearchKeepsPVAndHashFirst) {
    Move pv_move = Move(B4, C4);
    Move tt_move = Move(E2, E3);

    const auto moves =
        picked_moves(board, killers, history, ply, MovePickerMode::MainSearch, pv_move, tt_move);

    ASSERT_GT(moves.size(), 1U);
    EXPECT_EQ(moves[0], pv_move);
    EXPECT_EQ(moves[1], tt_move);
}

TEST_F(MovePickerTest, MainSearchKeepsGeneratedOrderForEqualPriorityMoves) {
    TestBoard quiet_board{STARTFEN};

    MoveList          generated = movegen::generate_pseudo_legal(quiet_board);
    std::vector<Move> generated_order(generated.begin(), generated.end());
    const auto        picked_order =
        picked_moves(quiet_board, killers, history, ply, MovePickerMode::MainSearch);

    EXPECT_EQ(picked_order, generated_order);
}

TEST_F(MovePickerTest, MainSearchIsDeterministicForEqualPriorityMoves) {
    TestBoard quiet_board{STARTFEN};

    const auto first_order =
        picked_moves(quiet_board, killers, history, ply, MovePickerMode::MainSearch);
    const auto second_order =
        picked_moves(quiet_board, killers, history, ply, MovePickerMode::MainSearch);

    ASSERT_GT(first_order.size(), 1U);
    EXPECT_EQ(second_order, first_order);
}

TEST_F(MovePickerTest, MainSearchKeepsHashMoveFirstWithoutPV) {
    TestBoard quiet_board{STARTFEN};
    Move      tt_move = Move(E2, E4);

    const auto moves = picked_moves(
        quiet_board, killers, history, ply, MovePickerMode::MainSearch, NULL_MOVE, tt_move);

    ASSERT_FALSE(moves.empty());
    EXPECT_EQ(moves[0], tt_move);
}

TEST_F(MovePickerTest, MainSearchKeepsPVHashKillerAndHistoryOrder) {
    Move pv_move     = Move(B4, C4);
    Move tt_move     = Move(E2, E3);
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves =
        picked_moves(board, killers, history, ply, MovePickerMode::MainSearch, pv_move, tt_move);

    ASSERT_GT(moves.size(), 4U);
    EXPECT_EQ(moves[0], pv_move);
    EXPECT_EQ(moves[1], tt_move);
    EXPECT_EQ(moves[2], Move(B4, F4));
    EXPECT_EQ(moves[3], killer_move);
    EXPECT_EQ(moves[4], hist_move);
}

TEST_F(MovePickerTest, MainSearchKeepsHashFirstAndDeterministicEqualPriorityOrder) {
    TestBoard quiet_board{STARTFEN};
    Move      tt_move = Move(E2, E4);

    const auto first_order = picked_moves(
        quiet_board, killers, history, ply, MovePickerMode::MainSearch, NULL_MOVE, tt_move);
    const auto second_order = picked_moves(
        quiet_board, killers, history, ply, MovePickerMode::MainSearch, NULL_MOVE, tt_move);

    ASSERT_GT(first_order.size(), 2U);
    EXPECT_EQ(first_order.front(), tt_move);
    EXPECT_EQ(second_order, first_order);
}

TEST_F(MovePickerTest, StagedMainSearchPicksSamePseudoLegalMoveSet) {
    for (std::string_view fen : {std::string_view{STARTFEN},
                                 std::string_view{POS2},
                                 std::string_view{POS3},
                                 std::string_view{ENPASSANT_A3},
                                 PROMOTION_FEN,
                                 CAPTURE_PROMOTION_FEN,
                                 CASTLE_FEN}) {
        TestBoard board{std::string(fen)};
        ASSERT_FALSE(board.is_check()) << fen;

        const auto staged = staged_picked_moves(board, killers, history, ply);
        MoveList   staged_list;
        for (Move move : staged)
            staged_list.add(move);

        EXPECT_EQ(sorted_move_bits(staged_list),
                  sorted_move_bits(movegen::generate_pseudo_legal(board)))
            << fen;
    }
}

TEST_F(MovePickerTest, MainSearchInCheckGeneratesEvasionsThroughMainSearchMode) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    ASSERT_TRUE(evasion_board.is_check());

    const auto moves =
        picked_moves(evasion_board, killers, history, ply, MovePickerMode::MainSearch);

    EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(movegen::generate_evasions(evasion_board)));
    EXPECT_NE(std::find(moves.begin(), moves.end(), Move(A8, B8)), moves.end());
}

TEST_F(MovePickerTest, MainSearchInCheckUsesDedicatedEvasionStage) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    Move      quiet_evasion = Move(A8, B8);
    killers.update(quiet_evasion, ply);
    ASSERT_TRUE(evasion_board.is_check());

    MovePicker picker{{evasion_board, killers, history, ply}, MovePickerMode::MainSearch};

    MovePickSource quiet_evasion_source = MovePickSource::None;
    for (Move picked = picker.next(); !picked.is_null(); picked = picker.next()) {
        if (picked == quiet_evasion) {
            quiet_evasion_source = picker.last_source();
            break;
        }
    }

    EXPECT_EQ(quiet_evasion_source, MovePickSource::Evasion);
}

TEST_F(MovePickerTest, MainSearchInCheckDoesNotReturnNonEvasionHintsAsHints) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    Move      non_evasion_king_move = Move(A8, A7);
    ASSERT_TRUE(evasion_board.is_check());
    ASSERT_TRUE(evasion_board.is_pseudo_legal(non_evasion_king_move));
    ASSERT_FALSE(evasion_board.is_legal_pseudo_move(non_evasion_king_move));

    MovePicker picker{
        {evasion_board, killers, history, ply, non_evasion_king_move, non_evasion_king_move},
        MovePickerMode::MainSearch};

    for (Move picked = picker.next(); !picked.is_null(); picked = picker.next()) {
        EXPECT_NE(picker.last_source(), MovePickSource::PV);
        EXPECT_NE(picker.last_source(), MovePickSource::Hash);
    }
}

TEST_F(MovePickerTest, StagedMainSearchKeepsPVAndHashFirst) {
    Move pv_move = Move(B4, C4);
    Move tt_move = Move(E2, E3);

    const auto moves = staged_picked_moves(board, killers, history, ply, pv_move, tt_move);

    ASSERT_GT(moves.size(), 1U);
    EXPECT_EQ(moves[0], pv_move);
    EXPECT_EQ(moves[1], tt_move);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), pv_move), 1);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), tt_move), 1);
}

TEST_F(MovePickerTest, StagedMainSearchSourcesFollowStagedContract) {
    Move pv_move     = Move(B4, C4);
    Move tt_move     = Move(E2, E3);
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves =
        staged_picked_moves_with_sources(board, killers, history, ply, pv_move, tt_move);

    ASSERT_GT(moves.size(), 4U);
    EXPECT_EQ(moves[0], std::make_pair(pv_move, MovePickSource::PV));
    EXPECT_EQ(moves[1], std::make_pair(tt_move, MovePickSource::Hash));
    EXPECT_EQ(moves[2], std::make_pair(Move(B4, F4), MovePickSource::GoodNoisy));
    EXPECT_EQ(moves[3], std::make_pair(killer_move, MovePickSource::Killer));
    EXPECT_EQ(moves[4], std::make_pair(hist_move, MovePickSource::Quiet));
}

TEST_F(MovePickerTest, StagedMainSearchMatchesGeneratedOrderForQuietStartPosition) {
    TestBoard quiet_board{STARTFEN};

    const auto              staged    = staged_picked_moves(quiet_board, killers, history, ply);
    const auto              generated = movegen::generate_pseudo_legal(quiet_board);
    const std::vector<Move> generated_order(generated.begin(), generated.end());

    EXPECT_EQ(staged, generated_order);
}

TEST_F(MovePickerTest, StagedMainSearchPriorityBandsAreOrderedForRepresentativePositions) {
    for (std::string_view fen : {std::string_view{STARTFEN},
                                 std::string_view{POS2},
                                 std::string_view{POS3},
                                 std::string_view{ENPASSANT_A3},
                                 PROMOTION_FEN,
                                 CAPTURE_PROMOTION_FEN,
                                 CASTLE_FEN}) {
        TestBoard board{std::string(fen)};
        ASSERT_FALSE(board.is_check()) << fen;

        const auto staged = staged_picked_moves_with_scores(board, killers, history, ply);
        const auto bands  = picked_priority_bands(staged);

        EXPECT_TRUE(std::is_sorted(bands.begin(), bands.end(), std::greater<>())) << fen;
    }
}

TEST_F(MovePickerTest, StagedMainSearchKeepsCaptureKillerAndHistoryPriorityBands) {
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto moves = staged_picked_moves(board, killers, history, ply);

    ASSERT_GT(moves.size(), 4U);
    EXPECT_EQ(moves[0], Move(B4, F4));
    EXPECT_EQ(moves[1], killer_move);
    EXPECT_EQ(moves[2], hist_move);
}

TEST_F(MovePickerTest, StagedMainSearchSuppressesKillerDuplicates) {
    TestBoard quiet_board{STARTFEN};
    Move      killer_1 = Move(E2, E4);
    Move      killer_2 = Move(D2, D4);

    killers.update(killer_2, ply);
    killers.update(killer_1, ply);

    const auto moves = staged_picked_moves(quiet_board, killers, history, ply);

    ASSERT_GT(moves.size(), 2U);
    EXPECT_EQ(moves[0], killer_1);
    EXPECT_EQ(moves[1], killer_2);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), killer_1), 1);
    EXPECT_EQ(std::count(moves.begin(), moves.end(), killer_2), 1);
}

TEST_F(MovePickerTest, StagedMainSearchKeepsWeakTacticalsAfterQuiets) {
    TestBoard board{std::string(WEAK_CAPTURE_FEN)};

    const auto moves    = staged_picked_moves_with_scores(board, killers, history, ply);
    const auto weak_it  = std::find_if(moves.begin(), moves.end(), [](const auto& move_and_score) {
        return move_and_score.first == Move(D1, D7) && move_and_score.second == PRIORITY_WEAK;
    });
    const auto quiet_it = std::find_if(moves.begin(), moves.end(), [&](const auto& move_and_score) {
        return move_and_score.first.type() != MOVE_PROM && !board.is_capture(move_and_score.first);
    });

    ASSERT_NE(weak_it, moves.end());
    ASSERT_NE(quiet_it, moves.end());
    EXPECT_LT(quiet_it, weak_it);
}

TEST_F(MovePickerTest, StagedMainSearchMarksWeakTacticalsAsBadNoisy) {
    TestBoard board{std::string(WEAK_CAPTURE_FEN)};

    const auto moves   = staged_picked_moves_with_sources(board, killers, history, ply);
    const auto weak_it = std::find_if(moves.begin(), moves.end(), [](const auto& move_and_source) {
        return move_and_source.first == Move(D1, D7);
    });

    ASSERT_NE(weak_it, moves.end());
    EXPECT_EQ(weak_it->second, MovePickSource::BadNoisy);
}

TEST_F(MovePickerTest, StagedMainSearchCoversSpecialMoveStages) {
    TestBoard promotion_board{std::string(PROMOTION_FEN)};
    TestBoard capture_promotion_board{std::string(CAPTURE_PROMOTION_FEN)};
    TestBoard castle_board{std::string(CASTLE_FEN)};
    TestBoard ep_board{ENPASSANT_A3};

    const auto promotions = staged_picked_moves(promotion_board, killers, history, ply);
    const auto capture_promotions =
        staged_picked_moves(capture_promotion_board, killers, history, ply);
    const auto castles = staged_picked_moves(castle_board, killers, history, ply);
    const auto eps     = staged_picked_moves(ep_board, killers, history, ply);

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

TEST_F(MovePickerTest, StagedMainSearchKeepsDeterministicEqualPriorityQuietOrder) {
    TestBoard quiet_board{STARTFEN};

    const auto first_order  = staged_picked_moves(quiet_board, killers, history, ply);
    const auto second_order = staged_picked_moves(quiet_board, killers, history, ply);

    ASSERT_GT(first_order.size(), 1U);
    EXPECT_EQ(second_order, first_order);
}

TEST_F(MovePickerTest, StagedMainSearchDefersQuietGenerationUntilAfterGoodNoisy) {
    MovePicker picker{{board, killers, history, ply}, MovePickerMode::MainSearch};

    EXPECT_FALSE(picker.noisy_generated());
    EXPECT_FALSE(picker.quiet_generated());

    Move first = picker.next();

    ASSERT_FALSE(first.is_null());
    EXPECT_EQ(first, Move(B4, F4));
    EXPECT_EQ(picker.last_source(), MovePickSource::GoodNoisy);
    EXPECT_TRUE(picker.noisy_generated());
    EXPECT_FALSE(picker.quiet_generated());
}

TEST_F(MovePickerTest, StagedMainSearchDefersQuietGenerationUntilAfterKillers) {
    TestBoard quiet_board{STARTFEN};
    Move      killer_move = Move(E2, E4);
    killers.update(killer_move, ply);

    MovePicker picker{{quiet_board, killers, history, ply}, MovePickerMode::MainSearch};

    Move first = picker.next();

    ASSERT_FALSE(first.is_null());
    EXPECT_EQ(first, killer_move);
    EXPECT_EQ(picker.last_source(), MovePickSource::Killer);
    EXPECT_TRUE(picker.noisy_generated());
    EXPECT_FALSE(picker.quiet_generated());
}

TEST_F(MovePickerTest, SkipQuietMovesSkipsRemainingQuietsAndKeepsBadNoisy) {
    TestBoard  weak_capture_board{std::string(WEAK_CAPTURE_FEN)};
    MovePicker picker{{weak_capture_board, killers, history, ply}, MovePickerMode::MainSearch};

    Move first_quiet = NULL_MOVE;
    for (Move picked = picker.next(); !picked.is_null(); picked = picker.next()) {
        if (picker.last_source() == MovePickSource::Quiet) {
            first_quiet = picked;
            break;
        }
    }

    ASSERT_FALSE(first_quiet.is_null());
    EXPECT_TRUE(picker.noisy_generated());
    EXPECT_TRUE(picker.quiet_generated());

    picker.skip_quiet_moves();

    Move next = picker.next();
    ASSERT_FALSE(next.is_null());
    EXPECT_EQ(next, Move(D1, D7));
    EXPECT_EQ(picker.last_source(), MovePickSource::BadNoisy);

    for (Move picked = picker.next(); !picked.is_null(); picked = picker.next())
        EXPECT_NE(picker.last_source(), MovePickSource::Quiet);
}

TEST_F(MovePickerTest, SkipQuietMovesDoesNotSuppressEvasions) {
    TestBoard evasion_board{std::string(QUIET_EVASION_FEN)};
    ASSERT_TRUE(evasion_board.is_check());

    MovePicker picker{{evasion_board, killers, history, ply}, MovePickerMode::MainSearch};
    picker.skip_quiet_moves();

    std::vector<Move> moves;
    for (Move picked = picker.next(); !picked.is_null(); picked = picker.next()) {
        moves.push_back(picked);
        EXPECT_EQ(picker.last_source(), MovePickSource::Evasion);
    }

    EXPECT_EQ(sorted_move_bits(moves), sorted_move_bits(movegen::generate_evasions(evasion_board)));
    EXPECT_NE(std::find(moves.begin(), moves.end(), Move(A8, B8)), moves.end());
}

TEST_F(MovePickerTest, StagedMainSearchKeepsExpectedPriorityBands) {
    Move pv_move     = Move(B4, C4);
    Move tt_move     = Move(E2, E3);
    Move killer_move = Move(A5, A4);
    Move hist_move   = Move(A5, A6);

    killers.update(killer_move, ply);
    history.update(board.side_to_move(), hist_move.from(), hist_move.to(), ply);

    const auto staged =
        staged_picked_moves_with_scores(board, killers, history, ply, pv_move, tt_move);
    const auto bands = picked_priority_bands(staged);

    ASSERT_GE(bands.size(), 5U);
    EXPECT_EQ(bands[0], PRIORITY_PV);
    EXPECT_EQ(bands[1], PRIORITY_HASH);
    EXPECT_EQ(bands[2], PRIORITY_CAPTURE);
    EXPECT_EQ(bands[3], PRIORITY_KILLER);
    EXPECT_EQ(bands[4], PRIORITY_HISTORY);
}
