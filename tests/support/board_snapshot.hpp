#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "board/board.hpp"

namespace board_test {

inline Bitboard piece_bits(const Board& board, Color color, PieceType piece) {
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
    std::string                                                  fen;
    Color                                                        side;
    CastleRights                                                 castle;
    Square                                                       enpassant;
    Square                                                       legal_enpassant;
    std::uint8_t                                                 halfmove;
    int                                                          fullmove;
    PositionKey                                                  key;
    Move                                                         previous_move;
    PieceType                                                    captured;
    Bitboard                                                     occupancy;
    Bitboard                                                     checkers;
    std::array<Bitboard, N_COLORS>                               blockers{};
    std::array<Bitboard, N_COLORS>                               pinners{};
    std::array<Bitboard, piece_slots>                            checking_squares{};
    std::array<Square, N_COLORS>                                 kings{};
    TaperedScore                                                 material;
    TaperedScore                                                 psq;
    std::array<Piece, N_SQUARES>                                 squares{};
    std::array<std::array<Bitboard, N_PIECETYPES>, N_COLORS>     piece_bb{};
    std::array<std::array<std::uint8_t, N_PIECETYPES>, N_COLORS> counts{};
};

inline BoardSnapshot snapshot_board(const Board& board) {
    BoardSnapshot snapshot{};

    snapshot.fen             = board.toFEN();
    snapshot.side            = board.side_to_move();
    snapshot.castle          = board.castle_rights();
    snapshot.enpassant       = board.enpassant_sq();
    snapshot.legal_enpassant = board.legal_enpassant_sq();
    snapshot.halfmove        = board.halfmove();
    snapshot.fullmove        = board.fullmove();
    snapshot.key             = board.key();
    snapshot.previous_move   = board.position_state().previous_move;
    snapshot.captured        = board.position_state().captured;
    snapshot.occupancy       = board.occupancy();
    snapshot.checkers        = board.checkers();
    snapshot.material        = board.material_score();
    snapshot.psq             = board.psq_bonus_score();

    for (int c = BLACK; c < N_COLORS; ++c) {
        const auto color                      = Color(c);
        snapshot.blockers[c]                  = board.blockers(color);
        snapshot.pinners[c]                   = board.pinners(color);
        snapshot.kings[c]                     = board.king_sq(color);
        snapshot.piece_bb[c][all_pieces_slot] = board.pieces(color);

        for (int p = PAWN; p <= KING; ++p) {
            snapshot.piece_bb[c][p] = piece_bits(board, color, PieceType(p));
            snapshot.counts[c][p]   = board.count(color, PieceType(p));
        }
    }

    for (int p = PAWN; p <= KING; ++p)
        snapshot.checking_squares[piece_slot(PieceType(p))] =
            board.position_state().checking_squares(PieceType(p));

    for (auto sq = A1; sq != INVALID; ++sq)
        snapshot.squares[sq] = board.piece_on(sq);

    return snapshot;
}

inline void expect_same_durable_representation(const Board& board, const BoardSnapshot& expected) {
    EXPECT_EQ(board.occupancy(), expected.occupancy);
    EXPECT_EQ(board.material_score(), expected.material);
    EXPECT_EQ(board.psq_bonus_score(), expected.psq);

    for (auto sq = A1; sq != INVALID; ++sq)
        EXPECT_EQ(board.piece_on(sq), expected.squares[sq]) << sq;

    for (int c = BLACK; c < N_COLORS; ++c) {
        const auto color = Color(c);
        EXPECT_EQ(board.king_sq(color), expected.kings[c]);
        EXPECT_EQ(board.pieces(color), expected.piece_bb[c][all_pieces_slot])
            << "color " << c << " all pieces";

        for (int p = PAWN; p <= KING; ++p) {
            EXPECT_EQ(piece_bits(board, color, PieceType(p)), expected.piece_bb[c][p])
                << "color " << c << " piece " << p;
            EXPECT_EQ(board.count(color, PieceType(p)), expected.counts[c][p])
                << "color " << c << " piece " << p;
        }
    }
}

inline void expect_same_board_snapshot(const Board& board, const BoardSnapshot& expected) {
    expect_same_durable_representation(board, expected);

    EXPECT_EQ(board.toFEN(), expected.fen);
    EXPECT_EQ(board.side_to_move(), expected.side);
    EXPECT_EQ(board.castle_rights(), expected.castle);
    EXPECT_EQ(board.enpassant_sq(), expected.enpassant);
    EXPECT_EQ(board.legal_enpassant_sq(), expected.legal_enpassant);
    EXPECT_EQ(board.halfmove(), expected.halfmove);
    EXPECT_EQ(board.fullmove(), expected.fullmove);
    EXPECT_EQ(board.key(), expected.key);
    EXPECT_EQ(board.position_state().previous_move, expected.previous_move);
    EXPECT_EQ(board.position_state().captured, expected.captured);
    EXPECT_EQ(board.checkers(), expected.checkers);

    for (int c = BLACK; c < N_COLORS; ++c) {
        EXPECT_EQ(board.blockers(Color(c)), expected.blockers[c]);
        EXPECT_EQ(board.pinners(Color(c)), expected.pinners[c]);
    }

    for (int p = PAWN; p <= KING; ++p) {
        EXPECT_EQ(board.position_state().checking_squares(PieceType(p)),
                  expected.checking_squares[piece_slot(PieceType(p))]);
    }
}

} // namespace board_test
