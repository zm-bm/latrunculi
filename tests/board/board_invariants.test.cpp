#include "board/board.hpp"
#include "board/ply_state.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <random>
#include <string_view>
#include <vector>

#include "core/attacks.hpp"
#include "movegen/movegen.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"
#include "support/board_snapshot.hpp"

namespace {

const std::array<std::string_view, 13> invariant_fens = {
    board_test::fen::start,
    board_test::fen::perft_position_2,
    board_test::fen::perft_position_3,
    board_test::fen::perft_position_4_white,
    board_test::fen::perft_position_4_black,
    board_test::fen::perft_position_5,
    board_test::fen::perft_position_6,
    board_test::fen::kings_only,
    board_test::fen::legal_en_passant_a3,
    board_test::fen::after_e2e4,
    board_test::fen::unhashable_en_passant_e3,
    board_test::fen::pinned_en_passant_e3,
    board_test::fen::white_pawn_on_a7,
};

void expect_tactical_cache_consistent(const Board& board) {
    const Color    us        = board.side_to_move();
    const Bitboard occupancy = board.occupancy();

    EXPECT_EQ(board.checkers(), board.attacks_to(board.king_sq(us), ~us)) << board.to_fen();
    for (int c = BLACK; c < N_COLORS; ++c) {
        const Color  king_color   = Color(c);
        const Color  sniper_color = ~king_color;
        const Square king         = board.king_sq(king_color);
        EXPECT_EQ(board.blockers(king_color),
                  attacks::slider_blockers(king,
                                           board.pieces<BISHOP, QUEEN>(sniper_color),
                                           board.pieces<ROOK, QUEEN>(sniper_color),
                                           occupancy))
            << board.to_fen();
    }
}

void expect_board_consistent(const Board& board) {
    std::array<std::array<Bitboard, N_PIECETYPES>, N_COLORS>     expected_piece_bb{};
    std::array<std::array<std::uint8_t, N_PIECETYPES>, N_COLORS> expected_counts{};
    std::array<Square, N_COLORS> expected_kings    = {INVALID, INVALID};
    TaperedScore                 expected_material = TaperedScore::Zero;
    TaperedScore                 expected_psq      = TaperedScore::Zero;

    for (auto sq = A1; sq != INVALID; ++sq) {
        const Piece piece = board.piece_on(sq);
        if (piece == NO_PIECE)
            continue;

        const Color     color = color_of(piece);
        const PieceType type  = type_of(piece);
        const Bitboard  sq_bb = bb::set(sq);

        expected_piece_bb[color][type]            |= sq_bb;
        expected_piece_bb[color][all_pieces_slot] |= sq_bb;
        expected_counts[color][type]++;
        expected_material += eval::piece(type, color);
        expected_psq      += eval::piece_sq(type, color, sq);
        if (type == KING)
            expected_kings[color] = sq;
    }

    for (int c = BLACK; c < N_COLORS; ++c) {
        const auto color = Color(c);
        ASSERT_NE(board.king_sq(color), INVALID) << c;
        EXPECT_EQ(board.king_sq(color), expected_kings[c]);
        EXPECT_EQ(board.piece_on(board.king_sq(color)), make_piece(color, KING));

        for (int p = PAWN; p <= KING; ++p) {
            const auto piece = PieceType(p);
            EXPECT_EQ(board_test::piece_bits(board, color, piece), expected_piece_bb[c][p])
                << "color " << c << " piece " << p;
            EXPECT_EQ(board.count(color, piece), expected_counts[c][p])
                << "color " << c << " piece " << p;
            EXPECT_EQ(board.count(color, piece),
                      bb::count(board_test::piece_bits(board, color, piece)))
                << "color " << c << " piece " << p;
        }
        EXPECT_EQ(board.pieces(color), expected_piece_bb[c][all_pieces_slot]);
    }

    EXPECT_EQ(board.pieces(WHITE) & board.pieces(BLACK), 0);
    EXPECT_EQ(board.occupancy(), board.pieces(WHITE) | board.pieces(BLACK));
    EXPECT_EQ(board.material_score(), expected_material);
    EXPECT_EQ(board.psq_bonus_score(), expected_psq);
    EXPECT_EQ(board.key(), board.recompute_key());
    expect_tactical_cache_consistent(board);
}

Move first_legal_move(Board& board) {
    for (const Move move : movegen::generate_pseudo_legal(board)) {
        if (board.is_legal_generated_move(move))
            return move;
    }
    return NULL_MOVE;
}

std::vector<Move> legal_moves(const Board& board) {
    std::vector<Move> legal;
    for (const Move move : movegen::generate_pseudo_legal(board)) {
        if (board.is_legal_generated_move(move))
            legal.push_back(move);
    }
    return legal;
}

} // namespace

TEST(BoardInvariantTest, EveryLegalTransitionPreservesInvariantsAndRestoresPosition) {
    for (const std::string_view fen : invariant_fens) {
        SCOPED_TRACE(fen);
        board_test::Harness board{fen};
        expect_board_consistent(board);
        const auto before = board_test::snapshot_board(board);

        for (const Move move : legal_moves(board)) {
            SCOPED_TRACE(move);
            board.make(move);
            expect_board_consistent(board);
            board.unmake();
            board_test::expect_same_board_snapshot(board, before);
            expect_board_consistent(board);
        }
    }
}

TEST(BoardInvariantTest, SeededMultiPlyMakeUnmakePreservesEveryPosition) {
    struct Playout {
        std::string_view fen;
        std::uint64_t    seed;
        int              target_plies;
    };

    const std::array<Playout, 3> playouts = {
        Playout{board_test::fen::start, 0x8D4A'12F3'902B'7C61ULL, 100},
        Playout{board_test::fen::perft_position_2, 0x19C0'5EED'4B71'A263ULL, 80},
        Playout{board_test::fen::perft_position_6, 0xA731'CC29'05E4'18BDULL, 80},
    };

    constexpr std::size_t minimum_plies = 50;

    for (const auto& playout : playouts) {
        SCOPED_TRACE(testing::Message() << "seed=" << playout.seed << " fen=" << playout.fen);

        board_test::Harness                    board{playout.fen};
        std::mt19937_64                        random{playout.seed};
        std::vector<board_test::BoardSnapshot> positions;
        positions.reserve(playout.target_plies);

        for (int ply = 0; ply < playout.target_plies; ++ply) {
            const auto moves = legal_moves(board);
            if (moves.empty()) {
                EXPECT_GE(positions.size(), minimum_plies)
                    << "terminal position at ply " << ply << ": " << board.to_fen();
                break;
            }

            positions.push_back(board_test::snapshot_board(board));
            std::uniform_int_distribution<std::size_t> select(0, moves.size() - 1);
            board.make(moves[select(random)]);
            expect_board_consistent(board);
        }

        EXPECT_GE(positions.size(), minimum_plies);
        for (auto position = positions.rbegin(); position != positions.rend(); ++position) {
            board.unmake();
            board_test::expect_same_board_snapshot(board, *position);
            expect_board_consistent(board);
        }
    }
}

TEST(BoardInvariantTest, CapturePromotionRestoresPosition) {
    board_test::Harness board(board_test::fen::capture_promotion);
    const auto          before = board_test::snapshot_board(board);
    const Move          move(A7, B8, MOVE_PROM, QUEEN);
    ASSERT_TRUE(board.is_legal_move(move));

    board.make(move);
    EXPECT_EQ(board.to_fen(), "1Q2k3/8/8/8/8/8/8/4K3 b - - 0 1");
    expect_board_consistent(board);

    board.unmake();
    board_test::expect_same_board_snapshot(board, before);
    expect_board_consistent(board);
}

TEST(BoardInvariantTest, NullMovePreservesDurableRepresentation) {
    const std::array<std::string_view, 4> fens = {board_test::fen::start,
                                                  board_test::fen::perft_position_2,
                                                  board_test::fen::legal_en_passant_a3,
                                                  board_test::fen::unhashable_en_passant_e3};

    for (const std::string_view fen : fens) {
        SCOPED_TRACE(fen);
        board_test::Harness board{fen};
        const auto          before          = board_test::snapshot_board(board);
        const int           fullmove_number = board.fullmove_number();

        board.make_null();
        board_test::expect_same_durable_representation(board, before);
        EXPECT_EQ(board.enpassant_target(), INVALID);
        EXPECT_EQ(board.legal_enpassant_target(), INVALID);
        EXPECT_EQ(board.fullmove_number(), fullmove_number);
        expect_board_consistent(board);

        board.unmake_null();
        board_test::expect_same_board_snapshot(board, before);
        expect_board_consistent(board);
    }
}

TEST(BoardInvariantTest, CallerOwnedMakeUnmakeRestoresPosition) {
    PlyState   root_state;
    Board      board(root_state, board_test::fen::perft_position_2);
    const auto before = board_test::snapshot_board(board);

    const Move first = first_legal_move(board);
    ASSERT_FALSE(first.is_null());
    PlyState first_state;
    board.make(first, first_state);
    const auto after_first = board_test::snapshot_board(board);
    expect_board_consistent(board);

    const Move second = first_legal_move(board);
    ASSERT_FALSE(second.is_null());
    PlyState second_state;
    board.make(second, second_state);
    expect_board_consistent(board);

    board.unmake(first_state);
    board_test::expect_same_board_snapshot(board, after_first);
    expect_board_consistent(board);

    board.unmake(root_state);
    board_test::expect_same_board_snapshot(board, before);
    expect_board_consistent(board);
}

TEST(BoardInvariantTest, CallerOwnedNullMoveRestoresPosition) {
    PlyState   root_state;
    Board      board(root_state, board_test::fen::perft_position_2);
    const auto before = board_test::snapshot_board(board);

    PlyState first_state;
    board.make_null(first_state);
    const auto after_first = board_test::snapshot_board(board);
    board_test::expect_same_durable_representation(board, before);
    expect_board_consistent(board);

    PlyState second_state;
    board.make_null(second_state);
    board_test::expect_same_durable_representation(board, before);
    expect_board_consistent(board);

    board.unmake_null(first_state);
    board_test::expect_same_board_snapshot(board, after_first);
    expect_board_consistent(board);

    board.unmake_null(root_state);
    board_test::expect_same_board_snapshot(board, before);
    expect_board_consistent(board);
}

TEST(BoardInvariantTest, UnmakeIrreversibleMovePreservesPriorRepetitionHistory) {
    board_test::Harness board("7k/8/8/8/8/8/P7/K7 w - - 0 1");

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
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
    expect_board_consistent(board);
}
