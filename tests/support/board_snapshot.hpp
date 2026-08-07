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
    Square                                                       legal_enpassant_target;
    PositionKey                                                  key;
    Move                                                         previous_move;
    bool                                                         can_unmake;
    Bitboard                                                     occupancy;
    Bitboard                                                     checkers;
    std::array<Bitboard, N_COLORS>                               blockers{};
    std::array<Square, N_COLORS>                                 kings{};
    eval::BaseTerms                                              base_terms;
    std::array<std::array<Bitboard, N_PIECETYPES>, N_COLORS>     piece_bb{};
    std::array<std::array<std::uint8_t, N_PIECETYPES>, N_COLORS> counts{};
};

inline BoardSnapshot snapshot_board(const Board& board) {
    BoardSnapshot snapshot{};

    snapshot.fen                    = board.to_fen();
    snapshot.legal_enpassant_target = board.legal_enpassant_target();
    snapshot.key                    = board.key();
    snapshot.previous_move          = board.previous_move();
    snapshot.can_unmake             = board.can_unmake();
    snapshot.occupancy              = board.occupancy();
    snapshot.checkers               = board.checkers();
    snapshot.base_terms             = board.base_terms();

    for (int c = BLACK; c < N_COLORS; ++c) {
        const auto color                      = Color(c);
        snapshot.blockers[c]                  = board.blockers(color);
        snapshot.kings[c]                     = board.king_sq(color);
        snapshot.piece_bb[c][all_pieces_slot] = board.pieces(color);

        for (int p = PAWN; p <= KING; ++p) {
            snapshot.piece_bb[c][p] = piece_bits(board, color, PieceType(p));
            snapshot.counts[c][p]   = board.count(color, PieceType(p));
        }
    }

    return snapshot;
}

inline void expect_base_terms_consistent(const Board& board) {
    EXPECT_EQ(board.base_terms(), board.recompute_base_terms());
}

inline void expect_same_board_snapshot(const Board& board, const BoardSnapshot& expected) {
    EXPECT_EQ(board.to_fen(), expected.fen);
    EXPECT_EQ(board.legal_enpassant_target(), expected.legal_enpassant_target);
    EXPECT_EQ(board.key(), expected.key);
    EXPECT_EQ(board.previous_move(), expected.previous_move);
    EXPECT_EQ(board.can_unmake(), expected.can_unmake);
    EXPECT_EQ(board.occupancy(), expected.occupancy);
    EXPECT_EQ(board.checkers(), expected.checkers);
    EXPECT_EQ(board.base_terms(), expected.base_terms);
    expect_base_terms_consistent(board);

    for (int c = BLACK; c < N_COLORS; ++c) {
        const auto color = Color(c);
        EXPECT_EQ(board.king_sq(color), expected.kings[c]);
        EXPECT_EQ(board.blockers(color), expected.blockers[c]);
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

} // namespace board_test
