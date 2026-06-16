#include "board.hpp"

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

#include "movegen.hpp"
#include "test_util.hpp"
#include "zobrist.hpp"

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

uint64_t piece_bits(const Board& board, Color color, PieceType piece) {
    switch (piece) {
    case ALL_PIECES: return board.pieces<ALL_PIECES>(color);
    case PAWN:       return board.pieces<PAWN>(color);
    case KNIGHT:     return board.pieces<KNIGHT>(color);
    case BISHOP:     return board.pieces<BISHOP>(color);
    case ROOK:       return board.pieces<ROOK>(color);
    case QUEEN:      return board.pieces<QUEEN>(color);
    case KING:       return board.pieces<KING>(color);
    default:         return 0;
    }
}

struct BoardSnapshot {
    std::string  fen;
    Color        side;
    CastleRights castle;
    Square       enpassant;
    Square       legal_enpassant;
    uint8_t      halfmove;
    uint32_t     fullmove;
    uint64_t     key;
    uint64_t     occupancy;
    uint64_t     checkers;
    uint64_t     blockers[N_COLORS];
    Square       kings[N_COLORS];
    Score        material;
    Score        psq;
    Piece        squares[N_SQUARES];
    uint64_t     piece_bb[N_COLORS][N_PIECES];
    uint8_t      counts[N_COLORS][N_PIECES];
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
        snap.kings[c]    = board.king_sq(color);

        for (int p = ALL_PIECES; p < N_PIECES; ++p)
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

        for (int p = ALL_PIECES; p < N_PIECES; ++p)
            EXPECT_EQ(piece_bits(board, color, PieceType(p)), snap.piece_bb[c][p])
                << "color " << c << " piece " << p;
        for (int p = PAWN; p <= KING; ++p)
            EXPECT_EQ(board.count(color, PieceType(p)), snap.counts[c][p])
                << "color " << c << " piece " << p;
    }
}

void expect_same_board_state(const Board& board, const BoardSnapshot& snap) {
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
    for (int c = BLACK; c < N_COLORS; ++c)
        EXPECT_EQ(board.blockers(Color(c)), snap.blockers[c]);
}

void expect_board_consistent(const Board& board) {
    uint64_t expected_piece_bb[N_COLORS][N_PIECES] = {};
    uint8_t  expected_counts[N_COLORS][N_PIECES]   = {};
    Square   expected_kings[N_COLORS]              = {INVALID, INVALID};
    Score    expected_material                     = Score::Zero;
    Score    expected_psq                          = Score::Zero;

    for (auto sq = A1; sq != INVALID; ++sq) {
        const Piece piece = board.piece_on(sq);
        if (piece == NO_PIECE)
            continue;

        const Color     color = color_of(piece);
        const PieceType type  = type_of(piece);
        const uint64_t  sq_bb = bb::set(sq);

        expected_piece_bb[color][type]       |= sq_bb;
        expected_piece_bb[color][ALL_PIECES] |= sq_bb;
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
        EXPECT_EQ(piece_bits(board, color, ALL_PIECES), expected_piece_bb[c][ALL_PIECES]);
    }

    EXPECT_EQ(piece_bits(board, WHITE, ALL_PIECES) & piece_bits(board, BLACK, ALL_PIECES), 0);
    EXPECT_EQ(board.occupancy(),
              piece_bits(board, WHITE, ALL_PIECES) | piece_bits(board, BLACK, ALL_PIECES));
    EXPECT_EQ(board.material_score(), expected_material);
    EXPECT_EQ(board.psq_bonus_score(), expected_psq);
    EXPECT_EQ(board.key(), board.calculate_key());
}

} // namespace

TEST(BoardRepresentationTest, EMPTYFEN) {
    Board b(EMPTYFEN);

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

    EXPECT_EQ(b.material_score(), Score::Zero);
    EXPECT_EQ(b.psq_bonus_score(), Score::Zero);

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

TEST(BoardRepresentationTest, STARTFEN) {
    Board b(STARTFEN);

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

    EXPECT_EQ(b.material_score(), Score::Zero);
    EXPECT_EQ(b.psq_bonus_score(), Score::Zero);

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
    Board b(POS4B);

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
    Board b("3rk3/8/8/8/8/8/8/3QK3 w - - 0 1");
    EXPECT_EQ(b.material_score(), eval::piece(QUEEN, WHITE) + eval::piece(ROOK, BLACK));
    EXPECT_EQ(b.psq_bonus_score(),
              eval::piece_sq(QUEEN, WHITE, D1) + eval::piece_sq(ROOK, BLACK, D8));
    expect_board_consistent(b);
}

TEST(BoardRepresentationTest, add_piece) {
    Board    board = Board(EMPTYFEN);
    uint64_t key   = board.key() ^ zob::hash_piece(WHITE, PAWN, E2);
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
    Board    board = Board(PAWN_E2);
    uint64_t key   = board.key() ^ zob::hash_piece(WHITE, PAWN, E2);
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
    Board    board  = Board(PAWN_E2);
    uint64_t key    = board.key();
    key            ^= zob::hash_piece(WHITE, PAWN, E2);
    key            ^= zob::hash_piece(WHITE, PAWN, E4);
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
        Board b(fen);
        EXPECT_EQ(b.key(), b.calculate_key());
    }
}

TEST(BoardRepresentationTest, NonPawnMaterial) {
    int mat = 2 * KNIGHT_MG + 2 * BISHOP_MG + 2 * ROOK_MG + QUEEN_MG;
    std::vector<std::tuple<std::string, Color, int>> test_cases = {
        {EMPTYFEN, WHITE, 0},
        {EMPTYFEN, BLACK, 0},
        {STARTFEN, WHITE, mat},
        {STARTFEN, BLACK, mat},
        {"4k3/8/8/8/8/8/8/4K1NR w K - 0 1", WHITE, KNIGHT_MG + ROOK_MG},
        {"4k1nr/8/8/8/8/8/8/4K3 w k - 0 1", BLACK, KNIGHT_MG + ROOK_MG},
    };

    for (const auto& [fen, color, expected] : test_cases) {
        Board board(fen);
        EXPECT_EQ(board.nonPawnMaterial(color), expected);
        expect_board_consistent(board);
    }
}

TEST(BoardRepresentationTest, LoadedFixturesAreConsistent) {
    for (const auto& fen : representation_fens) {
        Board board(fen);
        expect_board_consistent(board);
    }
}

TEST(BoardRepresentationTest, MakeUnmakePreservesRepresentation) {
    for (const auto& fen :
         {STARTFEN, POS2, POS3, POS4B, ENPASSANT_A3, "4k3/P7/8/8/8/8/8/4K3 w - - 0 1"}) {
        Board board(fen);
        auto  before   = snapshot(board);
        auto  movelist = generate<ALL_MOVES>(board);

        for (const auto& move : movelist) {
            if (!board.is_legal_pseudo_move(move))
                continue;

            board.make(move);
            expect_board_consistent(board);
            board.unmake();
            expect_same_board_state(board, before);
            expect_board_consistent(board);
        }
    }
}

TEST(BoardRepresentationTest, NullMovePreservesDurableRepresentation) {
    for (const auto& fen : {STARTFEN, POS2, ENPASSANT_A3, "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1"}) {
        Board board(fen);
        auto  before = snapshot(board);

        board.make_null();
        expect_same_durable_representation(board, before);
        expect_board_consistent(board);

        board.unmake_null();
        expect_same_board_state(board, before);
        expect_board_consistent(board);
    }
}
