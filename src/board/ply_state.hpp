#pragma once

#include <array>
#include <cstdint>

#include "board/castling_rights.hpp"
#include "core/bitboard.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

/**
 * Reversible position state and tactical data for one entry in Board's history
 * stack.
 *
 * Board owns and manages these states alongside its durable representation.
 */
struct PlyState {
    // Derived attack data refreshed after FEN load, make, and null moves.
    // Opposing pieces checking the side-to-move king.
    Bitboard checkers{};
    // For each king color, pieces that are the sole blocker between that king
    // and an opposing slider. A blocker may belong to either side.
    std::array<Bitboard, N_COLORS> blockers{};

    // Reversible position state.
    // Full Zobrist key; en passant is keyed only through legal_enpassant_target.
    PositionKey zkey{};
    // Remaining castling rights.
    CastlingRights castling_rights{NO_CASTLE};
    // FEN target after a double pawn push; it need not be capturable.
    Square enpassant_target{INVALID};
    // Same target when at least one legal en-passant capture exists; otherwise INVALID.
    // Move generation and Zobrist hashing use this value.
    Square legal_enpassant_target{INVALID};
    // Halfmove clock, reset by pawn moves and captures.
    std::uint8_t halfmove_clock{};

    // Undo data for the move that reached this ply.
    // Move recorded for this state; NULL_MOVE after FEN load or a null move.
    Move previous_move{NULL_MOVE};
    // Captured type removed by previous_move, or NO_PIECETYPE when none.
    PieceType captured_piece_type{NO_PIECETYPE};
};
