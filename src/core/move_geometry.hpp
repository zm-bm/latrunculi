#pragma once

#include "core/bitboard.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

enum CastleSide { CASTLE_KINGSIDE, CASTLE_QUEENSIDE, N_CASTLES };

namespace move_geometry {

struct CastlingGeometry {
    Square   king_from;
    Square   king_to;
    Square   rook_from;
    Square   rook_to;
    Bitboard empty_path;
    Bitboard king_path;
};

constexpr int pawn_push(Color color) noexcept {
    return color == WHITE ? square::north : square::south;
}

// Square occupied by the pawn removed when capturer moves to target.
constexpr Square enpassant_captured_square(Square target, Color capturer) noexcept {
    return target + -pawn_push(capturer);
}

// En-passant target created behind a pawn that lands on destination.
constexpr Square enpassant_target(Square destination, Color pusher) noexcept {
    return destination + -pawn_push(pusher);
}

// Requires the endpoints of an encoded castling move.
constexpr CastleSide castle_side(Square king_from, Square king_to) noexcept {
    return king_to > king_from ? CASTLE_KINGSIDE : CASTLE_QUEENSIDE;
}

constexpr const CastlingGeometry& castling(CastleSide side, Color color) noexcept {
    static constexpr CastlingGeometry table[N_CASTLES][N_COLORS] = {
        {
            // Kingside
            {
                // Black
                .king_from  = E8,
                .king_to    = G8,
                .rook_from  = H8,
                .rook_to    = F8,
                .empty_path = bb::set(F8, G8),
                .king_path  = bb::set(E8, F8, G8),
            },
            {
                // White
                .king_from  = E1,
                .king_to    = G1,
                .rook_from  = H1,
                .rook_to    = F1,
                .empty_path = bb::set(F1, G1),
                .king_path  = bb::set(E1, F1, G1),
            },
        },
        {
            // Queenside
            {
                // Black
                .king_from  = E8,
                .king_to    = C8,
                .rook_from  = A8,
                .rook_to    = D8,
                .empty_path = bb::set(B8, C8, D8),
                .king_path  = bb::set(C8, D8, E8),
            },
            {
                // White
                .king_from  = E1,
                .king_to    = C1,
                .rook_from  = A1,
                .rook_to    = D1,
                .empty_path = bb::set(B1, C1, D1),
                .king_path  = bb::set(C1, D1, E1),
            },
        },
    };

    return table[side][color];
}

} // namespace move_geometry
