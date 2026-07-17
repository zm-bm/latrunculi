#include "board/board.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"
#include "support/board_snapshot.hpp"

namespace {

constexpr std::string_view pawn_e4 = "4k3/8/8/8/4P3/8/8/4K3 w - - 0 1";

} // namespace

TEST(BoardRepresentationTest, BoardObjectSizesAreReported) {
    RecordProperty("PositionStateBytes", std::to_string(sizeof(PositionState)));
    RecordProperty("BoardBytes", std::to_string(sizeof(Board)));
}

TEST(BoardRepresentationTest, EmptyPositionEncoding) {
    board_test::Harness board(board_test::fen::kings_only);

    EXPECT_EQ(board.side_to_move(), WHITE);
    EXPECT_EQ(board.occupancy(), bb::set(E1, E8));
    EXPECT_EQ(board.king_sq(WHITE), E1);
    EXPECT_EQ(board.king_sq(BLACK), E8);
    EXPECT_EQ(board.piece_on(E1), W_KING);
    EXPECT_EQ(board.piece_on(E8), B_KING);
    EXPECT_EQ(board.piece_on(E2), NO_PIECE);

    for (const Color color : {BLACK, WHITE}) {
        EXPECT_EQ(board.pieces<KING>(color), bb::set(board.king_sq(color)));
        EXPECT_EQ(board.count(color, KING), 1);
        for (int p = PAWN; p < KING; ++p) {
            EXPECT_EQ(board_test::piece_bits(board, color, PieceType(p)), 0) << PieceType(p);
            EXPECT_EQ(board.count(color, PieceType(p)), 0) << PieceType(p);
        }
    }

    EXPECT_EQ(board.material_score(), TaperedScore::Zero);
    EXPECT_EQ(board.psq_bonus_score(), TaperedScore::Zero);
    EXPECT_EQ(board.castle_rights(), NO_CASTLE);
    EXPECT_EQ(board.checkers(), 0);
    EXPECT_EQ(board.blockers(WHITE), 0);
    EXPECT_EQ(board.blockers(BLACK), 0);
    EXPECT_EQ(board.enpassant_sq(), INVALID);
    EXPECT_EQ(board.key(), board.calculate_key());
}

TEST(BoardRepresentationTest, StartPositionEncoding) {
    board_test::Harness board(board_test::fen::start);

    EXPECT_EQ(board.side_to_move(), WHITE);
    EXPECT_EQ(board.king_sq(WHITE), E1);
    EXPECT_EQ(board.king_sq(BLACK), E8);
    EXPECT_EQ(board.pieces<PAWN>(WHITE), bb::rank(RANK2));
    EXPECT_EQ(board.pieces<PAWN>(BLACK), bb::rank(RANK7));
    EXPECT_EQ(board.pieces<KNIGHT>(WHITE), bb::set(B1, G1));
    EXPECT_EQ(board.pieces<KNIGHT>(BLACK), bb::set(B8, G8));
    EXPECT_EQ(board.pieces<BISHOP>(WHITE), bb::set(C1, F1));
    EXPECT_EQ(board.pieces<BISHOP>(BLACK), bb::set(C8, F8));
    EXPECT_EQ(board.pieces<ROOK>(WHITE), bb::set(A1, H1));
    EXPECT_EQ(board.pieces<ROOK>(BLACK), bb::set(A8, H8));
    EXPECT_EQ(board.pieces<QUEEN>(WHITE), bb::set(D1));
    EXPECT_EQ(board.pieces<QUEEN>(BLACK), bb::set(D8));
    EXPECT_EQ(board.pieces<KING>(WHITE), bb::set(E1));
    EXPECT_EQ(board.pieces<KING>(BLACK), bb::set(E8));
    EXPECT_EQ(board.occupancy(),
              bb::rank(RANK1) | bb::rank(RANK2) | bb::rank(RANK7) | bb::rank(RANK8));

    constexpr std::array<std::uint8_t, N_PIECETYPES> expected_counts = {0, 8, 2, 2, 2, 1, 1};
    for (const Color color : {BLACK, WHITE}) {
        for (int p = PAWN; p <= KING; ++p)
            EXPECT_EQ(board.count(color, PieceType(p)), expected_counts[p]) << PieceType(p);
    }

    EXPECT_EQ(board.piece_on(A2), W_PAWN);
    EXPECT_EQ(board.piece_on(F6), NO_PIECE);
    EXPECT_EQ(board.piecetype_on(A2), PAWN);
    EXPECT_EQ(board.piecetype_on(A3), NO_PIECETYPE);
    EXPECT_EQ(board.material_score(), TaperedScore::Zero);
    EXPECT_EQ(board.psq_bonus_score(), TaperedScore::Zero);
    EXPECT_EQ(board.castle_rights(), ALL_CASTLE);
    EXPECT_EQ(board.checkers(), 0);
    EXPECT_EQ(board.enpassant_sq(), INVALID);
    EXPECT_EQ(board.key(), board.calculate_key());
}

TEST(BoardRepresentationTest, AsymmetricPositionEncoding) {
    board_test::Harness board(board_test::fen::perft_position_4_black);

    EXPECT_EQ(board.side_to_move(), BLACK);
    EXPECT_EQ(board.king_sq(WHITE), E1);
    EXPECT_EQ(board.king_sq(BLACK), G8);
    EXPECT_EQ(board.piece_on(B7), W_PAWN);
    EXPECT_EQ(board.piece_on(A2), B_PAWN);
    EXPECT_EQ(board.piece_on(B3), W_BISHOP);
    EXPECT_EQ(board.piece_on(F6), B_KNIGHT);
    EXPECT_EQ(board.count(WHITE, PAWN), 7);
    EXPECT_EQ(board.count(BLACK, PAWN), 8);
    EXPECT_EQ(board.castle_rights(), W_CASTLE);
    EXPECT_EQ(board.checkers(), bb::set(B3));
    EXPECT_EQ(board.material_score(), eval::piece(PAWN, BLACK));
    EXPECT_LT(board.psq_bonus_score().mg, 0);
    EXPECT_EQ(board.key(), board.calculate_key());
}

TEST(BoardRepresentationTest, MaterialAndPsqtMatchExpectedValues) {
    board_test::Harness board("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");

    EXPECT_EQ(board.material_score(), eval::piece(QUEEN, WHITE) + eval::piece(ROOK, BLACK));
    EXPECT_EQ(board.psq_bonus_score(),
              eval::piece_sq(QUEEN, WHITE, D1) + eval::piece_sq(ROOK, BLACK, D8));
}

TEST(BoardRepresentationTest, AddPieceUpdatesDurableState) {
    board_test::Harness board(board_test::fen::kings_only);
    board.add_piece<true>(E2, WHITE, PAWN);
    board.update_check_data();

    EXPECT_EQ(board.piece_on(E2), W_PAWN);
    EXPECT_EQ(board.pieces<PAWN>(WHITE), bb::set(E2));
    EXPECT_EQ(board.count(WHITE, PAWN), 1);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E2, E1));
    EXPECT_EQ(board.toFEN(), board_test::fen::white_pawn_e2);
    EXPECT_EQ(board.key(), board.calculate_key());
}

TEST(BoardRepresentationTest, RemovePieceUpdatesDurableState) {
    board_test::Harness board(board_test::fen::white_pawn_e2);
    board.remove_piece<true>(E2, WHITE, PAWN);
    board.update_check_data();

    EXPECT_EQ(board.piece_on(E2), NO_PIECE);
    EXPECT_EQ(board.pieces<PAWN>(WHITE), 0);
    EXPECT_EQ(board.count(WHITE, PAWN), 0);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E1));
    EXPECT_EQ(board.toFEN(), board_test::fen::kings_only);
    EXPECT_EQ(board.key(), board.calculate_key());
}

TEST(BoardRepresentationTest, MovePieceUpdatesDurableState) {
    board_test::Harness board(board_test::fen::white_pawn_e2);
    board.move_piece<true>(E2, E4, WHITE, PAWN);
    board.update_check_data();

    EXPECT_EQ(board.piece_on(E2), NO_PIECE);
    EXPECT_EQ(board.piece_on(E4), W_PAWN);
    EXPECT_EQ(board.pieces<PAWN>(WHITE), bb::set(E4));
    EXPECT_EQ(board.count(WHITE, PAWN), 1);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E1, E4));
    EXPECT_EQ(board.toFEN(), pawn_e4);
    EXPECT_EQ(board.key(), board.calculate_key());
}

TEST(BoardRepresentationTest, NonPawnMaterialUsesPieceCounts) {
    const int start_material = 2 * piece_value::knight_mg + 2 * piece_value::bishop_mg +
                               2 * piece_value::rook_mg + piece_value::queen_mg;
    const std::vector<std::tuple<std::string_view, Color, int>> test_cases = {
        {board_test::fen::kings_only, WHITE, 0},
        {board_test::fen::kings_only, BLACK, 0},
        {board_test::fen::start, WHITE, start_material},
        {board_test::fen::start, BLACK, start_material},
        {"4k3/8/8/8/8/8/8/4K1NR w K - 0 1", WHITE, piece_value::knight_mg + piece_value::rook_mg},
        {"4k1nr/8/8/8/8/8/8/4K3 w k - 0 1", BLACK, piece_value::knight_mg + piece_value::rook_mg},
    };

    for (const auto& [fen, color, expected] : test_cases) {
        SCOPED_TRACE(fen);
        board_test::Harness board(fen);
        EXPECT_EQ(board.nonPawnMaterial(color), expected);
    }
}
