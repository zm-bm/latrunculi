#include "board/board.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>

#include "movegen/generator.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_snapshot.hpp"

namespace {

void complete_repetition_cycle(Board& board) {
    board.make(Move(E6, F5));
    board.make(Move(H7, G8));
    board.make(Move(F5, E6));
    board.make(Move(G8, H7));
}

void complete_corner_king_cycle(Board& board) {
    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
}

} // namespace

// Tactical state and check detection.

TEST(BoardRulesTest, CachesCheckersAndSliderBlockers) {
    struct Case {
        std::string_view fen;
        Bitboard         checkers;
        Bitboard         blockers;
    };
    constexpr std::array cases = {
        Case{"4r2k/8/8/8/8/8/8/4K3 w - - 0 1", bb::set(E8), 0},
        Case{"4r2k/8/8/8/8/8/4N3/4K3 w - - 0 1", 0, bb::set(E2)},
        Case{"4r2k/8/8/8/8/8/4n3/4K3 w - - 0 1", 0, bb::set(E2)},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(test.fen);
        Board board(test.fen);
        EXPECT_EQ(board.checkers(), test.checkers);
        EXPECT_EQ(board.blockers(WHITE), test.blockers);
    }
}

TEST(BoardRulesTest, GeometricAttacksIncludePinnedPieces) {
    Board board("4k3/8/8/8/r2N3K/8/8/8 w - - 0 1");

    EXPECT_EQ(board.blockers(WHITE), bb::set(D4));
    EXPECT_EQ(board.attacks_to(F5, WHITE), bb::set(D4));
    EXPECT_TRUE(board.any_attacked(bb::set(F5), WHITE));
    EXPECT_FALSE(board.is_legal_move(Move(D4, F5)));
}

TEST(BoardRulesTest, DetectsCheckingMoves) {
    Board board(board_test::fen::checking_move_candidates);
    EXPECT_TRUE(board.gives_check(Move(A1, A8)));
    EXPECT_TRUE(board.gives_check(Move(B1, G6)));
    EXPECT_TRUE(board.gives_check(Move(D1, A4)));
    EXPECT_TRUE(board.gives_check(Move(G4, F6)));
    EXPECT_FALSE(board.gives_check(Move(A1, A7)));
    EXPECT_FALSE(board.gives_check(Move(B1, H7)));
    EXPECT_FALSE(board.gives_check(Move(D1, F3)));
    EXPECT_FALSE(board.gives_check(Move(G4, H6)));
}

TEST(BoardRulesTest, DetectsDiscoveredChecks) {
    Board pieces("Q1N1k3/8/2N1N3/8/B7/8/4R3/4K3 w - - 0 1");
    EXPECT_TRUE(pieces.gives_check(Move(C8, B6)));
    EXPECT_TRUE(pieces.gives_check(Move(C6, B8)));
    EXPECT_TRUE(pieces.gives_check(Move(E6, C5)));

    Board king("4k3/8/8/8/8/8/4K3/4R3 w - - 0 1");
    EXPECT_TRUE(king.gives_check(Move(E2, D2)));
}

TEST(BoardRulesTest, EnPassantCanGiveDiscoveredCheck) {
    Board board("4k3/8/8/1pP5/B7/8/8/4K3 w - b6 0 1");
    EXPECT_TRUE(board.gives_check(Move(C5, B6, MOVE_EP)));
}

TEST(BoardRulesTest, PromotionCanGiveCheck) {
    Board board(board_test::fen::white_pawn_on_a7);
    EXPECT_TRUE(board.gives_check(Move(A7, A8, MOVE_PROM, QUEEN)));
    EXPECT_TRUE(board.gives_check(Move(A7, A8, MOVE_PROM, ROOK)));
    EXPECT_FALSE(board.gives_check(Move(A7, A8, MOVE_PROM, BISHOP)));
    EXPECT_FALSE(board.gives_check(Move(A7, A8, MOVE_PROM, KNIGHT)));
}

TEST(BoardRulesTest, CastlingCanGiveCheck) {
    Board board("5k2/8/8/8/8/8/8/4K2R w K - 0 1");
    EXPECT_TRUE(board.gives_check(Move(E1, G1, MOVE_CASTLE)));
}

// En passant state and hashing.

TEST(BoardRulesTest, CachesLegalEnPassantForBothColorsIncludingCheckEvasion) {
    struct Case {
        std::string_view with_enpassant;
        std::string_view without_enpassant;
        Square           enpassant_target;
        Move             move;
        bool             in_check;
    };

    constexpr std::array cases = {
        Case{board_test::fen::legal_en_passant_a3,
             "4k3/8/8/8/Pp6/8/8/4K3 b - - 0 1",
             A3,
             Move(B4, A3, MOVE_EP),
             false},
        Case{"7k/8/8/3Pp3/3K4/8/8/8 w - e6 0 1",
             "7k/8/8/3Pp3/3K4/8/8/8 w - - 0 1",
             E6,
             Move(D5, E6, MOVE_EP),
             true},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(test.with_enpassant);
        Board with_enpassant(test.with_enpassant);
        Board without_enpassant(test.without_enpassant);

        EXPECT_EQ(with_enpassant.is_check(), test.in_check);
        EXPECT_EQ(with_enpassant.enpassant_target(), test.enpassant_target);
        EXPECT_EQ(with_enpassant.legal_enpassant_target(), test.enpassant_target);
        EXPECT_EQ(with_enpassant.key(), with_enpassant.recompute_key());
        EXPECT_NE(with_enpassant.key(), without_enpassant.key());
        EXPECT_TRUE(with_enpassant.is_legal_move(test.move));

        const auto moves = movegen::generate_pseudo_legal(with_enpassant);
        EXPECT_NE(std::find(moves.begin(), moves.end(), test.move), moves.end());
    }
}

TEST(BoardRulesTest, AppliesLegalEnPassantCachePerCaptureOrigin) {
    Board      board("k2r4/8/8/3PpP2/8/8/8/3K4 w - e6 0 1");
    const Move pinned_capture(D5, E6, MOVE_EP);
    const Move legal_capture(F5, E6, MOVE_EP);

    EXPECT_EQ(board.legal_enpassant_target(), E6);
    EXPECT_TRUE(board.is_pseudo_legal(pinned_capture));
    EXPECT_TRUE(board.is_pseudo_legal(legal_capture));
    EXPECT_FALSE(board.is_legal_move(pinned_capture));
    EXPECT_TRUE(board.is_legal_move(legal_capture));

    const auto moves = movegen::generate_pseudo_legal(board);
    ASSERT_NE(std::find(moves.begin(), moves.end(), pinned_capture), moves.end());
    ASSERT_NE(std::find(moves.begin(), moves.end(), legal_capture), moves.end());
    EXPECT_FALSE(board.is_legal_pseudo_move(pinned_capture));
    EXPECT_TRUE(board.is_legal_pseudo_move(legal_capture));
}

TEST(BoardRulesTest, DoesNotCacheUnavailableEnPassantForEitherColor) {
    struct Case {
        std::string_view with_enpassant;
        std::string_view without_enpassant;
        Square           enpassant_target;
        Move             rejected_move;
    };

    constexpr std::array cases = {
        Case{board_test::fen::unhashable_en_passant_e3,
             "4k3/8/8/8/4P3/8/8/4K3 b - - 0 1",
             E3,
             NULL_MOVE},
        Case{board_test::fen::pinned_en_passant_e3,
             "8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - - 0 1",
             E3,
             Move(F4, E3, MOVE_EP)},
        Case{"k7/8/8/3pP3/8/5n2/8/4K3 w - d6 0 1",
             "k7/8/8/3pP3/8/5n2/8/4K3 w - - 0 1",
             D6,
             Move(E5, D6, MOVE_EP)},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(test.with_enpassant);
        Board with_enpassant(test.with_enpassant);
        Board without_enpassant(test.without_enpassant);

        EXPECT_EQ(with_enpassant.enpassant_target(), test.enpassant_target);
        EXPECT_EQ(with_enpassant.legal_enpassant_target(), INVALID);
        EXPECT_EQ(with_enpassant.key(), with_enpassant.recompute_key());
        EXPECT_EQ(with_enpassant.key(), without_enpassant.key());

        if (!test.rejected_move.is_null()) {
            EXPECT_TRUE(with_enpassant.is_pseudo_legal(test.rejected_move));
            EXPECT_FALSE(with_enpassant.is_legal_move(test.rejected_move));
            const auto moves = movegen::generate_pseudo_legal(with_enpassant);
            EXPECT_EQ(std::find(moves.begin(), moves.end(), test.rejected_move), moves.end());
        }
    }
}

// Move legality.

TEST(BoardRulesTest, FiltersPseudoLegalMovesThatLeaveTheKingUnsafe) {
    struct Case {
        std::string_view fen;
        Move             move;
        bool             legal;
    };

    constexpr std::array cases = {
        Case{board_test::fen::perft_position_3, Move(B5, B6), false},
        Case{board_test::fen::perft_position_3, Move(A5, B6), false},
        Case{board_test::fen::legal_en_passant_a3, Move(B4, A3, MOVE_EP), true},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(testing::Message() << test.fen << " move=" << test.move);
        Board board(test.fen);
        ASSERT_TRUE(board.is_pseudo_legal(test.move));
        EXPECT_EQ(board.is_legal_pseudo_move(test.move), test.legal);
        EXPECT_EQ(board.is_legal_move(test.move), test.legal);
    }
}

TEST(BoardRulesTest, RejectsUntrustedNonMoves) {
    struct Case {
        std::string_view fen;
        Move             move;
    };

    constexpr std::array cases = {
        Case{board_test::fen::quiet_black_to_move, Move(H1, H2)},
        Case{board_test::fen::start, NULL_MOVE},
        Case{board_test::fen::start, Move(E7, E5)},
        Case{board_test::fen::start, Move(G1, F3, MOVE_PROM, QUEEN)},
        Case{board_test::fen::start, Move(E2, E4, MOVE_CASTLE)},
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(testing::Message() << test.fen << " move=" << test.move);
        Board board(test.fen);
        EXPECT_FALSE(board.is_pseudo_legal(test.move));
        EXPECT_FALSE(board.is_legal_move(test.move));
    }
}

TEST(BoardRulesTest, ValidatesPawnMoveEncoding) {
    Board start{board_test::fen::start};
    EXPECT_TRUE(start.is_pseudo_legal(Move(E2, E4)));
    EXPECT_FALSE(start.is_pseudo_legal(Move(E2, E5)));

    Board blocked_double_push{"4k3/8/8/8/8/4N3/4P3/4K3 w - - 0 1"};
    EXPECT_FALSE(blocked_double_push.is_pseudo_legal(Move(E2, E4)));

    Board promotion{"7k/P7/8/8/8/8/8/4K3 w - - 0 1"};
    EXPECT_FALSE(promotion.is_pseudo_legal(Move(A7, A8)));
    EXPECT_TRUE(promotion.is_pseudo_legal(Move(A7, A8, MOVE_PROM, QUEEN)));

    Board enpassant{board_test::fen::legal_en_passant_a3};
    EXPECT_TRUE(enpassant.is_pseudo_legal(Move(B4, A3, MOVE_EP)));
    EXPECT_FALSE(enpassant.is_pseudo_legal(Move(B4, A3)));

    Board no_enpassant{"4k3/8/8/8/Pp6/8/8/4K3 b - - 0 1"};
    EXPECT_FALSE(no_enpassant.is_pseudo_legal(Move(B4, A3, MOVE_EP)));
}

TEST(BoardRulesTest, ValidatesCastlingConditions) {
    const Move kingside(E1, G1, MOVE_CASTLE);
    Board      castle_board{board_test::fen::perft_position_2};
    EXPECT_TRUE(castle_board.is_pseudo_legal(kingside));
    EXPECT_TRUE(castle_board.is_legal_move(kingside));
    EXPECT_TRUE(castle_board.is_pseudo_legal(Move(E1, C1, MOVE_CASTLE)));
    EXPECT_FALSE(castle_board.is_pseudo_legal(Move(E1, G1)));

    Board no_rights{"r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1"};
    EXPECT_FALSE(no_rights.is_pseudo_legal(kingside));

    Board missing_rook{"4k3/8/8/8/8/8/8/4K3 w K - 0 1"};
    EXPECT_FALSE(missing_rook.is_pseudo_legal(kingside));

    Board blocked_path{board_test::fen::start};
    EXPECT_FALSE(blocked_path.is_pseudo_legal(kingside));

    Board attacked_path{"r3k2r/8/8/8/8/8/5r2/R3K2R w KQkq - 0 1"};
    EXPECT_FALSE(attacked_path.is_pseudo_legal(kingside));

    Board in_check{"4r1k1/8/8/8/8/8/8/4K2R w K - 0 1"};
    EXPECT_FALSE(in_check.is_pseudo_legal(kingside));
}

TEST(BoardRulesTest, ValidatesLegalMovesWhileInCheck) {
    Board board(board_test::fen::perft_position_4_black);

    EXPECT_TRUE(board.is_check());
    EXPECT_TRUE(board.is_legal_move(Move(G8, H8)));
    EXPECT_TRUE(board.is_legal_move(Move(F6, D5)));
    EXPECT_TRUE(board.is_legal_move(Move(D7, D5)));
    EXPECT_FALSE(board.is_legal_move(Move(G8, F7)));
}

// Draw detection.

TEST(BoardRulesTest, AppliesFiftyMoveRule) {
    EXPECT_TRUE(Board("k7/8/8/8/8/8/8/K7 w - - 100 50").is_draw());
    EXPECT_FALSE(Board("k7/8/8/8/8/8/8/K7 w - - 99 50").is_draw());
}

TEST(BoardRulesTest, DetectsThreefoldRepetition) {
    Board board(board_test::fen::repetition_cycle);

    complete_repetition_cycle(board);
    board.make(Move(E6, F5));
    EXPECT_FALSE(board.is_draw());
    board.make(Move(H7, G8));
    EXPECT_FALSE(board.is_draw());
    board.make(Move(F5, E6));
    EXPECT_FALSE(board.is_draw());
    board.make(Move(G8, H7));
    EXPECT_FALSE(board.is_draw());
    board.make(Move(E6, F5));
    EXPECT_TRUE(board.is_draw());
}

TEST(BoardRulesTest, DistinguishesSearchCyclesFromGameHistoryRepetition) {
    Board board(board_test::fen::corner_kings);

    complete_corner_king_cycle(board);
    EXPECT_FALSE(board.is_draw());
    EXPECT_FALSE(board.is_draw(4));
    EXPECT_TRUE(board.is_draw(5));

    complete_corner_king_cycle(board);
    EXPECT_TRUE(board.is_draw());
}

TEST(BoardRulesTest, NullMoveUnmakeRestoresRepetitionHistory) {
    Board board(board_test::fen::corner_kings);

    complete_corner_king_cycle(board);
    EXPECT_FALSE(board.is_draw());

    board.make_null();
    board.unmake_null();
    EXPECT_FALSE(board.is_draw());

    complete_corner_king_cycle(board);
    EXPECT_TRUE(board.is_draw());
}

TEST(BoardRulesTest, UnmakeIrreversibleMovePreservesPriorRepetitionHistory) {
    Board board("7k/8/8/8/8/8/P7/K7 w - - 0 1");

    complete_corner_king_cycle(board);
    ASSERT_FALSE(board.is_draw());
    ASSERT_TRUE(board.is_draw(5));
    const auto before = board_test::snapshot_board(board);

    board.make(Move(A2, A3));
    board.make(Move(H8, G8));
    board.make(Move(A1, B1));
    EXPECT_FALSE(board.is_draw());

    board.unmake();
    board.unmake();
    board.unmake();

    board_test::expect_same_board_snapshot(board, before);
    EXPECT_FALSE(board.is_draw());
    EXPECT_TRUE(board.is_draw(5));
}
