#include "board/board.hpp"
#include "board/ply_state.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "movegen/movegen.hpp"
#include "support/board_fixtures.hpp"
#include "support/board_harness.hpp"
#include "support/board_snapshot.hpp"

namespace {

using Direction = std::pair<int, int>;

constexpr std::array<Direction, 8> knight_offsets = {
    Direction{1, 2},
    Direction{2, 1},
    Direction{2, -1},
    Direction{1, -2},
    Direction{-1, -2},
    Direction{-2, -1},
    Direction{-2, 1},
    Direction{-1, 2},
};

constexpr std::array<Direction, 8> king_offsets = {
    Direction{1, 1},
    Direction{1, 0},
    Direction{1, -1},
    Direction{0, -1},
    Direction{-1, -1},
    Direction{-1, 0},
    Direction{-1, 1},
    Direction{0, 1},
};

constexpr std::array<Direction, 4> bishop_directions = {
    Direction{1, 1}, Direction{1, -1}, Direction{-1, -1}, Direction{-1, 1}};

constexpr std::array<Direction, 4> rook_directions = {
    Direction{1, 0}, Direction{0, -1}, Direction{-1, 0}, Direction{0, 1}};

const std::array<std::string_view, 14> invariant_fens = {
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
    board_test::fen::multiple_pinners,
    board_test::fen::white_pawn_on_a7,
};

struct ExpectedCheckData {
    Bitboard                          checkers{};
    std::array<Bitboard, N_COLORS>    blockers{};
    std::array<Bitboard, N_COLORS>    pinners{};
    std::array<Bitboard, piece_slots> checks{};
};

int sign(int value) {
    return (value > 0) - (value < 0);
}

bool on_board(int file, int rank) {
    return file >= FILE1 && file <= FILE8 && rank >= RANK1 && rank <= RANK8;
}

Bitboard slow_leaper_attacks(Square from, std::span<const Direction> offsets) {
    Bitboard  result = 0;
    const int file   = square::file_of(from);
    const int rank   = square::rank_of(from);

    for (const auto& [df, dr] : offsets) {
        const int to_file = file + df;
        const int to_rank = rank + dr;
        if (on_board(to_file, to_rank))
            result |= bb::set(square::make(File(to_file), Rank(to_rank)));
    }

    return result;
}

Bitboard slow_knight_attacks(Square from) {
    return slow_leaper_attacks(from, knight_offsets);
}

Bitboard slow_king_attacks(Square from) {
    return slow_leaper_attacks(from, king_offsets);
}

Bitboard slow_pawn_attackers_to(Square target, Color attacker) {
    const int file = square::file_of(target);
    const int rank = square::rank_of(target);
    const int dr   = attacker == WHITE ? -1 : 1;

    Bitboard attackers = 0;
    for (const int df : {-1, 1}) {
        const int from_file = file + df;
        const int from_rank = rank + dr;
        if (on_board(from_file, from_rank))
            attackers |= bb::set(square::make(File(from_file), Rank(from_rank)));
    }

    return attackers;
}

bool ray_step(Square from, Square to, PieceType slider, int& df, int& dr) {
    const int  file_delta = square::file_of(to) - square::file_of(from);
    const int  rank_delta = square::rank_of(to) - square::rank_of(from);
    const bool diagonal   = std::abs(file_delta) == std::abs(rank_delta);
    const bool straight   = file_delta == 0 || rank_delta == 0;

    if (file_delta == 0 && rank_delta == 0)
        return false;

    if ((slider == BISHOP || slider == QUEEN) && diagonal) {
        df = sign(file_delta);
        dr = sign(rank_delta);
        return true;
    }

    if ((slider == ROOK || slider == QUEEN) && straight) {
        df = sign(file_delta);
        dr = sign(rank_delta);
        return true;
    }

    return false;
}

Bitboard slow_between(Square from, Square to) {
    int df = 0;
    int dr = 0;
    if (!ray_step(from, to, QUEEN, df, dr))
        return 0;

    Bitboard between = 0;
    int      file    = square::file_of(from) + df;
    int      rank    = square::rank_of(from) + dr;
    while (file != square::file_of(to) || rank != square::rank_of(to)) {
        between |= bb::set(square::make(File(file), Rank(rank)));
        file    += df;
        rank    += dr;
    }

    return between;
}

Bitboard slow_sliding_attacks(Square from, PieceType slider, Bitboard occupancy) {
    Bitboard result = 0;
    auto     scan   = [&](std::span<const Direction> directions) {
        for (const auto& [df, dr] : directions) {
            int file = square::file_of(from) + df;
            int rank = square::rank_of(from) + dr;
            while (on_board(file, rank)) {
                const Square sq  = square::make(File(file), Rank(rank));
                result          |= bb::set(sq);
                if (bb::contains(occupancy, sq))
                    break;
                file += df;
                rank += dr;
            }
        }
    };

    if (slider == BISHOP || slider == QUEEN)
        scan(bishop_directions);
    if (slider == ROOK || slider == QUEEN)
        scan(rook_directions);

    return result;
}

bool is_slider(PieceType piece) {
    return piece == BISHOP || piece == ROOK || piece == QUEEN;
}

Bitboard slow_attackers_to(const Board& board, Square target, Color attacker) {
    const Bitboard occupancy = board.occupancy();

    return (board.pieces<PAWN>(attacker) & slow_pawn_attackers_to(target, attacker)) |
           (board.pieces<KNIGHT>(attacker) & slow_knight_attacks(target)) |
           (board.pieces<KING>(attacker) & slow_king_attacks(target)) |
           (board.pieces<BISHOP, QUEEN>(attacker) &
            slow_sliding_attacks(target, BISHOP, occupancy)) |
           (board.pieces<ROOK, QUEEN>(attacker) & slow_sliding_attacks(target, ROOK, occupancy));
}

ExpectedCheckData expected_check_data(const Board& board) {
    ExpectedCheckData expected;
    const Color       us        = board.side_to_move();
    const Color       them      = ~us;
    const Square      them_king = board.king_sq(them);
    const Bitboard    occupancy = board.occupancy();

    expected.checkers                   = slow_attackers_to(board, board.king_sq(us), them);
    expected.checks[piece_slot(PAWN)]   = slow_pawn_attackers_to(them_king, us);
    expected.checks[piece_slot(KNIGHT)] = slow_knight_attacks(them_king);
    expected.checks[piece_slot(BISHOP)] = slow_sliding_attacks(them_king, BISHOP, occupancy);
    expected.checks[piece_slot(ROOK)]   = slow_sliding_attacks(them_king, ROOK, occupancy);
    expected.checks[piece_slot(QUEEN)] =
        expected.checks[piece_slot(BISHOP)] | expected.checks[piece_slot(ROOK)];

    for (int c = BLACK; c < N_COLORS; ++c) {
        const Color  king_color   = Color(c);
        const Color  sniper_color = ~king_color;
        const Square king         = board.king_sq(king_color);
        Bitboard     snipers      = 0;

        for (auto sq = A1; sq != INVALID; ++sq) {
            const Piece piece = board.piece_on(sq);
            if (piece == NO_PIECE || color_of(piece) != sniper_color || !is_slider(type_of(piece)))
                continue;

            int df = 0;
            int dr = 0;
            if (ray_step(king, sq, type_of(piece), df, dr))
                snipers |= bb::set(sq);
        }

        const Bitboard occupancy_without_snipers = occupancy ^ snipers;
        while (snipers) {
            const Square   sniper         = bb::lsb_pop(snipers);
            const Bitboard pieces_between = occupancy_without_snipers & slow_between(king, sniper);

            if (bb::is_one(pieces_between)) {
                expected.blockers[king_color] |= pieces_between;
                if (pieces_between & board.pieces(king_color))
                    expected.pinners[sniper_color] |= bb::set(sniper);
            }
        }
    }

    return expected;
}

void expect_check_data_matches_slow_oracle(const Board& board) {
    const ExpectedCheckData expected = expected_check_data(board);

    EXPECT_EQ(board.checkers(), expected.checkers) << board.toFEN();
    for (int c = BLACK; c < N_COLORS; ++c) {
        EXPECT_EQ(board.blockers(Color(c)), expected.blockers[c]) << board.toFEN();
        EXPECT_EQ(board.pinners(Color(c)), expected.pinners[c]) << board.toFEN();
    }
    for (int p = PAWN; p <= KING; ++p) {
        EXPECT_EQ(board.ply_state().tactical.checking_squares(PieceType(p)),
                  expected.checks[piece_slot(PieceType(p))])
            << board.toFEN() << " piece " << PieceType(p);
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
    EXPECT_EQ(board.key(), board.calculate_key());
    expect_check_data_matches_slow_oracle(board);
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
                    << "terminal position at ply " << ply << ": " << board.toFEN();
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
    EXPECT_EQ(board.toFEN(), "1Q2k3/8/8/8/8/8/8/4K3 b - - 0 1");
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
        const auto          before = board_test::snapshot_board(board);

        board.make_null();
        board_test::expect_same_durable_representation(board, before);
        expect_board_consistent(board);

        board.unmake_null();
        board_test::expect_same_board_snapshot(board, before);
        expect_board_consistent(board);
    }
}

TEST(BoardInvariantTest, CallerOwnedMakeUnmakeRestoresPosition) {
    PlyState   root_state;
    Board      board(root_state, std::string(board_test::fen::perft_position_2));
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
    Board      board(root_state, std::string(board_test::fen::perft_position_2));
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

TEST(BoardInvariantTest, LoadBoardPreservesRepetitionHistory) {
    board_test::Harness board(board_test::fen::repetition_cycle);

    board.make(Move(E6, F5));
    board.make(Move(H7, G8));
    board.make(Move(F5, E6));
    board.make(Move(G8, H7));
    board.make(Move(E6, F5));
    board.make(Move(H7, G8));
    board.make(Move(F5, E6));
    board.make(Move(G8, H7));
    board.make(Move(E6, F5));
    ASSERT_TRUE(board.is_draw());
    const auto before = board_test::snapshot_board(board);

    board_test::Harness fen_only(board.toFEN());
    EXPECT_FALSE(fen_only.is_draw());

    board_test::Harness clone(board_test::fen::start);
    clone.load_board(&board);

    EXPECT_TRUE(clone.is_draw());
    board_test::expect_same_board_snapshot(clone, before);
    expect_board_consistent(clone);
}
