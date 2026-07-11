#include "board/board.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "board/zobrist.hpp"
#include "movegen/movegen.hpp"
#include "support/test_util.hpp"

namespace {

const std::string representation_fens[] = {
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
};

Bitboard piece_bits(const Board& board, Color color, PieceType piece) {
    switch (piece) {
    case PAWN:   return board.pieces<PAWN>(color);
    case KNIGHT: return board.pieces<KNIGHT>(color);
    case BISHOP: return board.pieces<BISHOP>(color);
    case ROOK:   return board.pieces<ROOK>(color);
    case QUEEN:  return board.pieces<QUEEN>(color);
    case KING:   return board.pieces<KING>(color);
    default:     return 0;
    }
}

struct BoardSnapshot {
    std::string  fen;
    Color        side;
    CastleRights castle;
    Square       enpassant;
    Square       legal_enpassant;
    std::uint8_t halfmove;
    int          fullmove;
    PositionKey  key;
    Bitboard     occupancy;
    Bitboard     checkers;
    Bitboard     blockers[N_COLORS];
    Bitboard     pinners[N_COLORS];
    Square       kings[N_COLORS];
    TaperedScore material;
    TaperedScore psq;
    Piece        squares[N_SQUARES];
    Bitboard     piece_bb[N_COLORS][N_PIECETYPES];
    std::uint8_t counts[N_COLORS][N_PIECETYPES];
};

BoardSnapshot snapshot(const Board& board) {
    BoardSnapshot snap{};

    snap.fen             = board.toFEN();
    snap.side            = board.side_to_move();
    snap.castle          = board.castle_rights();
    snap.enpassant       = board.enpassant_sq();
    snap.legal_enpassant = board.legal_enpassant_sq();
    snap.halfmove        = board.halfmove();
    snap.fullmove        = board.fullmove();
    snap.key             = board.key();
    snap.occupancy       = board.occupancy();
    snap.checkers        = board.checkers();
    snap.material        = board.material_score();
    snap.psq             = board.psq_bonus_score();

    for (int c = BLACK; c < N_COLORS; ++c) {
        const auto color = Color(c);
        snap.blockers[c] = board.blockers(color);
        snap.pinners[c]  = board.pinners(color);
        snap.kings[c]    = board.king_sq(color);

        snap.piece_bb[c][all_pieces_slot] = board.pieces(color);
        for (int p = PAWN; p <= KING; ++p)
            snap.piece_bb[c][p] = piece_bits(board, color, PieceType(p));
        for (int p = PAWN; p <= KING; ++p)
            snap.counts[c][p] = board.count(color, PieceType(p));
    }

    for (auto sq = A1; sq != INVALID; ++sq)
        snap.squares[sq] = board.piece_on(sq);

    return snap;
}

void expect_same_durable_representation(const Board& board, const BoardSnapshot& snap) {
    EXPECT_EQ(board.occupancy(), snap.occupancy);
    EXPECT_EQ(board.material_score(), snap.material);
    EXPECT_EQ(board.psq_bonus_score(), snap.psq);

    for (auto sq = A1; sq != INVALID; ++sq)
        EXPECT_EQ(board.piece_on(sq), snap.squares[sq]) << sq;

    for (int c = BLACK; c < N_COLORS; ++c) {
        const auto color = Color(c);
        EXPECT_EQ(board.king_sq(color), snap.kings[c]);

        EXPECT_EQ(board.pieces(color), snap.piece_bb[c][all_pieces_slot])
            << "color " << c << " all pieces";
        for (int p = PAWN; p <= KING; ++p)
            EXPECT_EQ(piece_bits(board, color, PieceType(p)), snap.piece_bb[c][p])
                << "color " << c << " piece " << p;
        for (int p = PAWN; p <= KING; ++p)
            EXPECT_EQ(board.count(color, PieceType(p)), snap.counts[c][p])
                << "color " << c << " piece " << p;
    }
}

void expect_same_board_snapshot(const Board& board, const BoardSnapshot& snap) {
    expect_same_durable_representation(board, snap);

    EXPECT_EQ(board.toFEN(), snap.fen);
    EXPECT_EQ(board.side_to_move(), snap.side);
    EXPECT_EQ(board.castle_rights(), snap.castle);
    EXPECT_EQ(board.enpassant_sq(), snap.enpassant);
    EXPECT_EQ(board.legal_enpassant_sq(), snap.legal_enpassant);
    EXPECT_EQ(board.halfmove(), snap.halfmove);
    EXPECT_EQ(board.fullmove(), snap.fullmove);
    EXPECT_EQ(board.key(), snap.key);
    EXPECT_EQ(board.checkers(), snap.checkers);
    for (int c = BLACK; c < N_COLORS; ++c) {
        EXPECT_EQ(board.blockers(Color(c)), snap.blockers[c]);
        EXPECT_EQ(board.pinners(Color(c)), snap.pinners[c]);
    }
}

void expect_board_consistent(const Board& board) {
    Bitboard     expected_piece_bb[N_COLORS][N_PIECETYPES] = {};
    std::uint8_t expected_counts[N_COLORS][N_PIECETYPES]   = {};
    Square       expected_kings[N_COLORS]                  = {INVALID, INVALID};
    TaperedScore expected_material                         = TaperedScore::Zero;
    TaperedScore expected_psq                              = TaperedScore::Zero;

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
            EXPECT_EQ(piece_bits(board, color, piece), expected_piece_bb[c][p])
                << "color " << c << " piece " << p;
            EXPECT_EQ(board.count(color, piece), expected_counts[c][p])
                << "color " << c << " piece " << p;
            EXPECT_EQ(board.count(color, piece), bb::count(piece_bits(board, color, piece)))
                << "color " << c << " piece " << p;
        }
        EXPECT_EQ(board.pieces(color), expected_piece_bb[c][all_pieces_slot]);
    }

    EXPECT_EQ(board.pieces(WHITE) & board.pieces(BLACK), 0);
    EXPECT_EQ(board.occupancy(), board.pieces(WHITE) | board.pieces(BLACK));
    EXPECT_EQ(board.material_score(), expected_material);
    EXPECT_EQ(board.psq_bonus_score(), expected_psq);
    EXPECT_EQ(board.key(), board.calculate_key());
}

struct ExpectedCheckData {
    Bitboard checkers            = 0;
    Bitboard blockers[N_COLORS]  = {0};
    Bitboard pinners[N_COLORS]   = {0};
    Bitboard checks[piece_slots] = {0};
};

int sign(int value) {
    return (value > 0) - (value < 0);
}

bool on_board(int file, int rank) {
    return file >= FILE1 && file <= FILE8 && rank >= RANK1 && rank <= RANK8;
}

Bitboard slow_leaper_attacks(Square from, const std::vector<std::pair<int, int>>& offsets) {
    Bitboard  attacks = 0;
    const int file    = square::file_of(from);
    const int rank    = square::rank_of(from);

    for (const auto& [df, dr] : offsets) {
        const int to_file = file + df;
        const int to_rank = rank + dr;
        if (on_board(to_file, to_rank))
            attacks |= bb::set(square::make(File(to_file), Rank(to_rank)));
    }

    return attacks;
}

Bitboard slow_knight_attacks(Square from) {
    return slow_leaper_attacks(
        from, {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}});
}

Bitboard slow_king_attacks(Square from) {
    return slow_leaper_attacks(
        from, {{1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {0, 1}});
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
    const std::vector<std::pair<int, int>> bishop_dirs = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
    const std::vector<std::pair<int, int>> rook_dirs   = {{1, 0}, {0, -1}, {-1, 0}, {0, 1}};

    Bitboard attacks = 0;
    auto     scan    = [&](const std::vector<std::pair<int, int>>& dirs) {
        for (const auto& [df, dr] : dirs) {
            int file = square::file_of(from) + df;
            int rank = square::rank_of(from) + dr;
            while (on_board(file, rank)) {
                const Square sq  = square::make(File(file), Rank(rank));
                attacks         |= bb::set(sq);
                if (occupancy & bb::set(sq))
                    break;
                file += df;
                rank += dr;
            }
        }
    };

    if (slider == BISHOP || slider == QUEEN)
        scan(bishop_dirs);
    if (slider == ROOK || slider == QUEEN)
        scan(rook_dirs);

    return attacks;
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
        Bitboard       remaining_snipers         = snipers;
        while (remaining_snipers) {
            const Square   sniper         = bb::lsb_pop(remaining_snipers);
            const Bitboard pieces_between = occupancy_without_snipers & slow_between(king, sniper);

            if (pieces_between && !bb::is_many(pieces_between)) {
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
        EXPECT_EQ(board.position_state().checks[piece_slot(PieceType(p))],
                  expected.checks[piece_slot(PieceType(p))])
            << board.toFEN() << " piece " << PieceType(p);
    }
}

Move first_legal_move(Board& board) {
    auto movelist = movegen::generate_pseudo_legal(board);
    for (const auto& move : movelist) {
        if (board.is_legal_generated_move(move))
            return move;
    }
    return NULL_MOVE;
}

} // namespace

TEST(BoardRepresentationTest, EMPTYFEN) {
    TestBoard b(EMPTYFEN);

    EXPECT_EQ(b.king_sq(WHITE), E1);
    EXPECT_EQ(b.king_sq(BLACK), E8);
    EXPECT_EQ(b.side_to_move(), WHITE);
    EXPECT_EQ(b.pieces<KING>(WHITE), bb::set(E1));
    EXPECT_EQ(b.pieces<KING>(BLACK), bb::set(E8));
    EXPECT_EQ(b.pieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<PAWN>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<KNIGHT>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<KNIGHT>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<BISHOP>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<BISHOP>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<ROOK>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<ROOK>(BLACK), 0x0);
    EXPECT_EQ(b.pieces<QUEEN>(WHITE), 0x0);
    EXPECT_EQ(b.pieces<QUEEN>(BLACK), 0x0);

    EXPECT_EQ(b.occupancy(), bb::set(E1, E8));

    EXPECT_EQ(b.count(WHITE, KING), 1);
    EXPECT_EQ(b.count(BLACK, KING), 1);
    EXPECT_EQ(b.count(WHITE, PAWN), 0);
    EXPECT_EQ(b.count(BLACK, PAWN), 0);
    EXPECT_EQ(b.count(WHITE, KNIGHT), 0);
    EXPECT_EQ(b.count(BLACK, KNIGHT), 0);
    EXPECT_EQ(b.count(WHITE, BISHOP), 0);
    EXPECT_EQ(b.count(BLACK, BISHOP), 0);
    EXPECT_EQ(b.count(WHITE, ROOK), 0);
    EXPECT_EQ(b.count(BLACK, ROOK), 0);
    EXPECT_EQ(b.count(WHITE, QUEEN), 0);
    EXPECT_EQ(b.count(BLACK, QUEEN), 0);

    EXPECT_EQ(b.piece_on(E1), W_KING);
    EXPECT_EQ(b.piece_on(FILE5, RANK1), W_KING);
    EXPECT_EQ(b.piece_on(E2), NO_PIECE);
    EXPECT_EQ(b.piece_on(FILE5, RANK2), NO_PIECE);
    EXPECT_EQ(b.piecetype_on(E1), KING);
    EXPECT_EQ(b.piecetype_on(E2), NO_PIECETYPE);

    EXPECT_EQ(b.material_score(), TaperedScore::Zero);
    EXPECT_EQ(b.psq_bonus_score(), TaperedScore::Zero);

    EXPECT_EQ(b.castle_rights(), NO_CASTLE);
    EXPECT_FALSE(b.can_castle(WHITE));
    EXPECT_FALSE(b.can_castle(BLACK));
    EXPECT_FALSE(b.can_castle_kingside(WHITE));
    EXPECT_FALSE(b.can_castle_queenside(WHITE));
    EXPECT_FALSE(b.can_castle_kingside(BLACK));
    EXPECT_FALSE(b.can_castle_queenside(BLACK));

    EXPECT_EQ(b.checkers(), 0);
    EXPECT_EQ(b.blockers(WHITE), 0);
    EXPECT_EQ(b.blockers(BLACK), 0);
    EXPECT_EQ(b.enpassant_sq(), INVALID);

    EXPECT_FALSE(b.is_check());
    expect_board_consistent(b);
}

TEST(BoardRepresentationTest, BoardObjectStaysCompact) {
    EXPECT_EQ(sizeof(PositionState), 104U);
    EXPECT_LT(sizeof(Board), 4096U);
}

TEST(BoardRepresentationTest, STARTFEN) {
    TestBoard b(STARTFEN);

    EXPECT_EQ(b.king_sq(WHITE), E1);
    EXPECT_EQ(b.king_sq(BLACK), E8);
    EXPECT_EQ(b.side_to_move(), WHITE);
    EXPECT_EQ(b.pieces<KING>(WHITE), bb::set(E1));
    EXPECT_EQ(b.pieces<KING>(BLACK), bb::set(E8));
    EXPECT_EQ(b.pieces<PAWN>(WHITE), bb::rank(RANK2));
    EXPECT_EQ(b.pieces<PAWN>(BLACK), bb::rank(RANK7));
    EXPECT_EQ(b.pieces<KNIGHT>(WHITE), bb::set(B1, G1));
    EXPECT_EQ(b.pieces<KNIGHT>(BLACK), bb::set(B8, G8));
    EXPECT_EQ(b.pieces<BISHOP>(WHITE), bb::set(C1, F1));
    EXPECT_EQ(b.pieces<BISHOP>(BLACK), bb::set(C8, F8));
    EXPECT_EQ(b.pieces<ROOK>(WHITE), bb::set(A1, H1));
    EXPECT_EQ(b.pieces<ROOK>(BLACK), bb::set(A8, H8));
    EXPECT_EQ(b.pieces<QUEEN>(WHITE), bb::set(D1));
    EXPECT_EQ(b.pieces<QUEEN>(BLACK), bb::set(D8));

    EXPECT_EQ(b.occupancy(), bb::rank(RANK1) | bb::rank(RANK2) | bb::rank(RANK7) | bb::rank(RANK8));

    EXPECT_EQ(b.count(WHITE, KING), 1);
    EXPECT_EQ(b.count(BLACK, KING), 1);
    EXPECT_EQ(b.count(WHITE, PAWN), 8);
    EXPECT_EQ(b.count(BLACK, PAWN), 8);
    EXPECT_EQ(b.count(WHITE, KNIGHT), 2);
    EXPECT_EQ(b.count(BLACK, KNIGHT), 2);
    EXPECT_EQ(b.count(WHITE, BISHOP), 2);
    EXPECT_EQ(b.count(BLACK, BISHOP), 2);
    EXPECT_EQ(b.count(WHITE, ROOK), 2);
    EXPECT_EQ(b.count(BLACK, ROOK), 2);
    EXPECT_EQ(b.count(WHITE, QUEEN), 1);
    EXPECT_EQ(b.count(BLACK, QUEEN), 1);

    EXPECT_EQ(b.piece_on(A2), W_PAWN);
    EXPECT_EQ(b.piece_on(FILE1, RANK2), W_PAWN);
    EXPECT_EQ(b.piece_on(A3), NO_PIECE);
    EXPECT_EQ(b.piece_on(FILE1, RANK3), NO_PIECE);
    EXPECT_EQ(b.piecetype_on(A2), PAWN);
    EXPECT_EQ(b.piecetype_on(A3), NO_PIECETYPE);

    EXPECT_EQ(b.material_score(), TaperedScore::Zero);
    EXPECT_EQ(b.psq_bonus_score(), TaperedScore::Zero);

    EXPECT_EQ(b.castle_rights(), ALL_CASTLE);
    EXPECT_TRUE(b.can_castle(WHITE));
    EXPECT_TRUE(b.can_castle(BLACK));
    EXPECT_TRUE(b.can_castle_kingside(WHITE));
    EXPECT_TRUE(b.can_castle_queenside(WHITE));
    EXPECT_TRUE(b.can_castle_kingside(BLACK));
    EXPECT_TRUE(b.can_castle_queenside(BLACK));

    EXPECT_EQ(b.checkers(), 0);
    EXPECT_EQ(b.blockers(WHITE), 0);
    EXPECT_EQ(b.blockers(BLACK), 0);
    EXPECT_EQ(b.enpassant_sq(), INVALID);

    EXPECT_FALSE(b.is_check());
    expect_board_consistent(b);
}

TEST(BoardRepresentationTest, POS4B) {
    TestBoard b(POS4B);

    EXPECT_EQ(b.king_sq(WHITE), E1);
    EXPECT_EQ(b.king_sq(BLACK), G8);
    EXPECT_EQ(b.side_to_move(), BLACK);
    EXPECT_EQ(b.pieces<KING>(WHITE), bb::set(E1));
    EXPECT_EQ(b.pieces<KING>(BLACK), bb::set(G8));
    EXPECT_EQ(b.pieces<PAWN>(WHITE), bb::set(H2, G2, F2, D2, C2, B2, B7));
    EXPECT_EQ(b.pieces<PAWN>(BLACK), bb::set(H7, G7, E5, D7, C5, B4, A7, A2));
    EXPECT_EQ(b.pieces<KNIGHT>(WHITE), bb::set(A4, F3));
    EXPECT_EQ(b.pieces<KNIGHT>(BLACK), bb::set(F6, H3));
    EXPECT_EQ(b.pieces<BISHOP>(WHITE), bb::set(G3, B3));
    EXPECT_EQ(b.pieces<BISHOP>(BLACK), bb::set(B5, A5));
    EXPECT_EQ(b.pieces<ROOK>(WHITE), bb::set(H1, A1));
    EXPECT_EQ(b.pieces<ROOK>(BLACK), bb::set(F8, A8));
    EXPECT_EQ(b.pieces<QUEEN>(WHITE), bb::set(A6));
    EXPECT_EQ(b.pieces<QUEEN>(BLACK), bb::set(D8));

    EXPECT_EQ(b.count(WHITE, KING), 1);
    EXPECT_EQ(b.count(BLACK, KING), 1);
    EXPECT_EQ(b.count(WHITE, PAWN), 7);
    EXPECT_EQ(b.count(BLACK, PAWN), 8);
    EXPECT_EQ(b.count(WHITE, KNIGHT), 2);
    EXPECT_EQ(b.count(BLACK, KNIGHT), 2);
    EXPECT_EQ(b.count(WHITE, BISHOP), 2);
    EXPECT_EQ(b.count(BLACK, BISHOP), 2);
    EXPECT_EQ(b.count(WHITE, ROOK), 2);
    EXPECT_EQ(b.count(BLACK, ROOK), 2);
    EXPECT_EQ(b.count(WHITE, QUEEN), 1);
    EXPECT_EQ(b.count(BLACK, QUEEN), 1);

    EXPECT_EQ(b.piece_on(B3), W_BISHOP);
    EXPECT_EQ(b.piece_on(FILE6, RANK6), B_KNIGHT);
    EXPECT_EQ(b.piece_on(D4), NO_PIECE);
    EXPECT_EQ(b.piece_on(FILE3, RANK3), NO_PIECE);
    EXPECT_EQ(b.piecetype_on(B7), PAWN);
    EXPECT_EQ(b.piecetype_on(E2), NO_PIECETYPE);

    EXPECT_EQ(b.material_score(), eval::piece(PAWN, BLACK));
    EXPECT_LT(b.psq_bonus_score().mg, 0);

    EXPECT_EQ(b.castle_rights(), W_CASTLE);
    EXPECT_TRUE(b.can_castle(WHITE));
    EXPECT_FALSE(b.can_castle(BLACK));
    EXPECT_TRUE(b.can_castle_kingside(WHITE));
    EXPECT_TRUE(b.can_castle_queenside(WHITE));
    EXPECT_FALSE(b.can_castle_kingside(BLACK));
    EXPECT_FALSE(b.can_castle_queenside(BLACK));

    EXPECT_EQ(b.checkers(), bb::set(B3));
    EXPECT_EQ(b.blockers(WHITE), 0);
    EXPECT_EQ(b.blockers(BLACK), 0);
    EXPECT_EQ(b.enpassant_sq(), INVALID);

    EXPECT_TRUE(b.is_check());
    EXPECT_FALSE(b.is_double_check());
    expect_board_consistent(b);
}

TEST(BoardRepresentationTest, material_psq_bonus) {
    TestBoard b("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");
    EXPECT_EQ(b.material_score(), eval::piece(QUEEN, WHITE) + eval::piece(ROOK, BLACK));
    EXPECT_EQ(b.psq_bonus_score(),
              eval::piece_sq(QUEEN, WHITE, D1) + eval::piece_sq(ROOK, BLACK, D8));
    expect_board_consistent(b);
}

TEST(BoardRepresentationTest, add_piece) {
    TestBoard   board = TestBoard(EMPTYFEN);
    PositionKey key   = board.key() ^ zob::hash_piece(WHITE, PAWN, E2);
    board.add_piece<true>(E2, WHITE, PAWN);

    EXPECT_EQ(board.piece_on(E2), make_piece(WHITE, PAWN));
    EXPECT_EQ(board.pieces<PAWN>(WHITE), bb::set(E2));
    EXPECT_EQ(board.count(WHITE, PAWN), 1);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E2, E1));
    EXPECT_EQ(board.key(), key);
    EXPECT_EQ(board.toFEN(), PAWN_E2);
    expect_board_consistent(board);
}

TEST(BoardRepresentationTest, remove_piece) {
    TestBoard   board = TestBoard(PAWN_E2);
    PositionKey key   = board.key() ^ zob::hash_piece(WHITE, PAWN, E2);
    board.remove_piece<true>(E2, WHITE, PAWN);

    EXPECT_EQ(board.piece_on(E2), NO_PIECE);
    EXPECT_EQ(board.pieces<PAWN>(WHITE), 0x0);
    EXPECT_EQ(board.count(WHITE, PAWN), 0);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E1));
    EXPECT_EQ(board.key(), key);
    EXPECT_EQ(board.toFEN(), EMPTYFEN);
    expect_board_consistent(board);
}

TEST(BoardRepresentationTest, move_piece) {
    TestBoard   board  = TestBoard(PAWN_E2);
    PositionKey key    = board.key();
    key               ^= zob::hash_piece(WHITE, PAWN, E2);
    key               ^= zob::hash_piece(WHITE, PAWN, E4);
    board.move_piece<true>(E2, E4, WHITE, PAWN);

    EXPECT_EQ(board.piece_on(E2), NO_PIECE);
    EXPECT_EQ(board.piece_on(E4), make_piece(WHITE, PAWN));
    EXPECT_EQ(board.pieces<PAWN>(WHITE), bb::set(E4));
    EXPECT_EQ(board.count(WHITE, PAWN), 1);
    EXPECT_EQ(board.occupancy(), bb::set(E8, E1, E4));
    EXPECT_EQ(board.key(), key);
    EXPECT_EQ(board.toFEN(), PAWN_E4);
    expect_board_consistent(board);
}

TEST(BoardRepresentationTest, ZobristKey) {
    for (auto fen : representation_fens) {
        TestBoard b(fen);
        EXPECT_EQ(b.key(), b.calculate_key());
    }
}

TEST(BoardRepresentationTest, NonPawnMaterial) {
    int mat = 2 * piece_value::knight_mg + 2 * piece_value::bishop_mg + 2 * piece_value::rook_mg +
              piece_value::queen_mg;
    std::vector<std::tuple<std::string, Color, int>> test_cases = {
        {EMPTYFEN, WHITE, 0},
        {EMPTYFEN, BLACK, 0},
        {STARTFEN, WHITE, mat},
        {STARTFEN, BLACK, mat},
        {"4k3/8/8/8/8/8/8/4K1NR w K - 0 1", WHITE, piece_value::knight_mg + piece_value::rook_mg},
        {"4k1nr/8/8/8/8/8/8/4K3 w k - 0 1", BLACK, piece_value::knight_mg + piece_value::rook_mg},
    };

    for (const auto& [fen, color, expected] : test_cases) {
        TestBoard board(fen);
        EXPECT_EQ(board.nonPawnMaterial(color), expected);
        expect_board_consistent(board);
    }
}

TEST(BoardRepresentationTest, LoadedFixturesAreConsistent) {
    for (const auto& fen : representation_fens) {
        TestBoard board(fen);
        expect_board_consistent(board);
    }
}

TEST(BoardRepresentationTest, MakeUnmakePreservesRepresentation) {
    for (const auto& fen :
         {STARTFEN, POS2, POS3, POS4B, ENPASSANT_A3, "4k3/P7/8/8/8/8/8/4K3 w - - 0 1"}) {
        TestBoard board(fen);
        auto      before   = snapshot(board);
        auto      movelist = movegen::generate_pseudo_legal(board);

        for (const auto& move : movelist) {
            if (!board.is_legal_generated_move(move))
                continue;

            board.make(move);
            expect_board_consistent(board);
            board.unmake();
            expect_same_board_snapshot(board, before);
            expect_board_consistent(board);
        }
    }
}

TEST(BoardRepresentationTest, CapturePromotionMakeUnmakePreservesRepresentation) {
    const std::string fen = "1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    TestBoard         board(fen);
    const auto        before = snapshot(board);

    const Move move(A7, B8, MOVE_PROM, QUEEN);
    ASSERT_TRUE(board.is_legal_move(move));

    board.make(move);
    EXPECT_EQ(board.toFEN(), "1Q2k3/8/8/8/8/8/8/4K3 b - - 0 1");
    expect_board_consistent(board);

    board.unmake();
    expect_same_board_snapshot(board, before);
    expect_board_consistent(board);
}

TEST(BoardRepresentationTest, CheckDataMatchesSlowOracle) {
    const std::vector<std::string> fens = {
        STARTFEN,
        POS2,
        POS3,
        POS4B,
        ENPASSANT_A3,
        "8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - e3 0 1",
        "4r2k/4q3/8/8/8/8/4N3/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/4R3/4K3 w - - 0 1",
    };

    for (const auto& fen : fens) {
        TestBoard board(fen);
        SCOPED_TRACE(fen);
        expect_check_data_matches_slow_oracle(board);

        const auto movelist = movegen::generate_pseudo_legal(board);
        for (const auto& move : movelist) {
            if (!board.is_legal_generated_move(move))
                continue;

            SCOPED_TRACE(move);
            const auto before = snapshot(board);
            board.make(move);
            expect_check_data_matches_slow_oracle(board);
            board.unmake();
            expect_same_board_snapshot(board, before);
        }

        if (!board.is_check()) {
            const auto before = snapshot(board);
            board.make_null();
            expect_check_data_matches_slow_oracle(board);
            board.unmake_null();
            expect_same_board_snapshot(board, before);
        }
    }
}

TEST(BoardRepresentationTest, NullMovePreservesDurableRepresentation) {
    for (const auto& fen : {STARTFEN, POS2, ENPASSANT_A3, "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1"}) {
        TestBoard board(fen);
        auto      before = snapshot(board);

        board.make_null();
        expect_same_durable_representation(board, before);
        expect_board_consistent(board);

        board.unmake_null();
        expect_same_board_snapshot(board, before);
        expect_board_consistent(board);
    }
}

TEST(BoardRepresentationTest, CallerOwnedMakeUnmakePreservesRepresentation) {
    PositionState root_state;
    Board         board(root_state, POS2);
    auto          before = snapshot(board);

    Move first = first_legal_move(board);
    ASSERT_FALSE(first.is_null());
    PositionState first_state;
    board.make(first, first_state);
    auto after_first = snapshot(board);
    expect_board_consistent(board);

    Move second = first_legal_move(board);
    ASSERT_FALSE(second.is_null());
    PositionState second_state;
    board.make(second, second_state);
    expect_board_consistent(board);

    board.unmake(first_state);
    expect_same_board_snapshot(board, after_first);
    expect_board_consistent(board);

    board.unmake(root_state);
    expect_same_board_snapshot(board, before);
    expect_board_consistent(board);
}

TEST(BoardRepresentationTest, CallerOwnedNullMovePreservesRepresentation) {
    PositionState root_state;
    Board         board(root_state, POS2);
    auto          before = snapshot(board);

    PositionState first_state;
    board.make_null(first_state);
    auto after_first = snapshot(board);
    expect_same_durable_representation(board, before);
    expect_board_consistent(board);

    PositionState second_state;
    board.make_null(second_state);
    expect_same_durable_representation(board, before);
    expect_board_consistent(board);

    board.unmake_null(first_state);
    expect_same_board_snapshot(board, after_first);
    expect_board_consistent(board);

    board.unmake_null(root_state);
    expect_same_board_snapshot(board, before);
    expect_board_consistent(board);
}

TEST(BoardRepresentationTest, UnmakeIrreversibleMovePreservesPriorRepetitionHistory) {
    TestBoard board("7k/8/8/8/8/8/P7/K7 w - - 0 1");

    board.make(Move(A1, B1));
    board.make(Move(H8, G8));
    board.make(Move(B1, A1));
    board.make(Move(G8, H8));
    ASSERT_FALSE(board.is_draw());
    ASSERT_TRUE(board.is_draw(5));
    const auto before = snapshot(board);

    board.make(Move(A2, A3));
    board.make(Move(H8, G8));
    board.make(Move(A1, B1));
    EXPECT_FALSE(board.is_draw());

    board.unmake();
    board.unmake();
    board.unmake();

    expect_same_board_snapshot(board, before);
    EXPECT_FALSE(board.is_draw());
    EXPECT_TRUE(board.is_draw(5));
    expect_board_consistent(board);
}

TEST(BoardRepresentationTest, LoadBoardPreservesRepetitionHistory) {
    TestBoard board("3r4/ppq4k/1nb1BQ1p/4Pp1p/1b6/8/PP3PPP/2R1R1K1 w - - 2 26");

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
    const auto before = snapshot(board);

    TestBoard fen_only(board.toFEN());
    EXPECT_FALSE(fen_only.is_draw());

    TestBoard clone(STARTFEN);
    clone.load_board(&board);

    EXPECT_EQ(clone.toFEN(), board.toFEN());
    EXPECT_EQ(clone.key(), board.key());
    EXPECT_TRUE(clone.is_draw());
    expect_same_board_snapshot(clone, before);
    expect_board_consistent(clone);
}
