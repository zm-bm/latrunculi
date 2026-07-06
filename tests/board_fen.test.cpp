#include "board/board.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "test_util.hpp"

namespace {

const std::string round_trip_fens[] = {
    STARTFEN,
    POS2,
    POS3,
    POS4W,
    POS4B,
    POS5,
    POS6,
    EMPTYFEN,
    ENPASSANT_A3,
    E2E4,
    "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1",
};

void expect_same_reloaded_state(const std::string& fen) {
    TestBoard board(fen);
    TestBoard reloaded(board.toFEN());

    EXPECT_EQ(reloaded.toFEN(), board.toFEN());
    EXPECT_EQ(reloaded.side_to_move(), board.side_to_move());
    EXPECT_EQ(reloaded.castle_rights(), board.castle_rights());
    EXPECT_EQ(reloaded.enpassant_sq(), board.enpassant_sq());
    EXPECT_EQ(reloaded.halfmove(), board.halfmove());
    EXPECT_EQ(reloaded.fullmove(), board.fullmove());
    EXPECT_EQ(reloaded.material_score(), board.material_score());
    EXPECT_EQ(reloaded.psq_bonus_score(), board.psq_bonus_score());
    EXPECT_EQ(reloaded.checkers(), board.checkers());
    EXPECT_EQ(reloaded.blockers(WHITE), board.blockers(WHITE));
    EXPECT_EQ(reloaded.blockers(BLACK), board.blockers(BLACK));
    EXPECT_EQ(reloaded.key(), board.key());
    EXPECT_EQ(reloaded.key(), reloaded.calculate_key());

    for (auto sq = A1; sq != INVALID; ++sq)
        EXPECT_EQ(reloaded.piece_on(sq), board.piece_on(sq)) << sq;

    for (int c = BLACK; c < N_COLORS; ++c)
        for (int p = PAWN; p <= KING; ++p)
            EXPECT_EQ(reloaded.count(Color(c), PieceType(p)), board.count(Color(c), PieceType(p)));
}

} // namespace

TEST(BoardFenTest, LoadsAndOutputsCorrectFens) {
    for (auto fen : round_trip_fens) {
        EXPECT_EQ(TestBoard(fen).toFEN(), fen) << "should return identical fen";
    }
}

TEST(BoardFenTest, FourFieldFenNormalizesClocks) {
    EXPECT_EQ(TestBoard("4k3/8/8/8/8/8/8/4K3 w - -").toFEN(), EMPTYFEN);
    EXPECT_EQ(TestBoard("4k3/8/8/8/8/8/8/4K3 b - -").toFEN(), "4k3/8/8/8/8/8/8/4K3 b - - 0 1");
}

TEST(BoardFenTest, MaxHalfmoveAndLongFullmoveFensRoundTrip) {
    const std::string white = "4k3/8/8/8/8/8/8/4K3 w - - 255 300";
    const std::string black = "4k3/8/8/8/8/8/8/4K3 b - - 255 300";

    EXPECT_EQ(TestBoard(white).toFEN(), white);
    EXPECT_EQ(TestBoard(black).toFEN(), black);
    EXPECT_EQ(+TestBoard(white).halfmove(), 255);
    EXPECT_EQ(TestBoard(white).fullmove(), 300);
}

TEST(BoardFenTest, PreservesRawUnhashableEnPassantSquare) {
    const std::string fen = "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1";
    TestBoard         board(fen);

    EXPECT_EQ(board.enpassant_sq(), E3);
    EXPECT_EQ(board.legal_enpassant_sq(), INVALID);
    EXPECT_EQ(board.toFEN(), fen);
}

TEST(BoardFenTest, InvalidFenDoesNotMutateBoard) {
    TestBoard board(E2E4);

    const auto  fen      = board.toFEN();
    const auto  key      = board.key();
    const Score material = board.material_score();
    const Score psq      = board.psq_bonus_score();

    const std::vector<std::string> invalid_fens = {
        "4k3/8/8/8/8/8/8/4K3 w - - 0",
        "4k03/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 x - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w QK - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - e3 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 256 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 2147483649",
    };

    for (const auto& invalid_fen : invalid_fens) {
        EXPECT_THROW(board.load_fen(invalid_fen), std::invalid_argument) << invalid_fen;
        EXPECT_EQ(board.toFEN(), fen);
        EXPECT_EQ(board.key(), key);
        EXPECT_EQ(board.material_score(), material);
        EXPECT_EQ(board.psq_bonus_score(), psq);
        EXPECT_EQ(board.key(), board.calculate_key());
    }
}

TEST(BoardFenTest, ReloadingFenReproducesLoadedPosition) {
    const std::vector<std::string> fens = {
        STARTFEN,
        POS2,
        ENPASSANT_A3,
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 10 20",
        "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 255 300",
    };

    for (const auto& fen : fens)
        expect_same_reloaded_state(fen);
}
