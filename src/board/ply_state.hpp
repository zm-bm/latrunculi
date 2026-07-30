#pragma once

#include <array>
#include <cstdint>

#include "board/castling_rights.hpp"
#include "core/bitboard.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

/** One entry in Board's reversible history stack. */
struct PlyState {
    // Cached tactical data for this position.
    // Opposing pieces checking the side-to-move king.
    Bitboard checkers{};
    // For each king color, pieces that are the sole blocker between that king
    // and an opposing slider. A blocker may belong to either side.
    std::array<Bitboard, N_COLORS> blockers{};

    // Rule state and position key.
    // Full Zobrist key; en passant is keyed only through legal_enpassant_target.
    PositionKey    zkey{};
    CastlingRights castling_rights{NO_CASTLE};
    // FEN target after a double pawn push; it need not be capturable.
    Square enpassant_target{INVALID};
    // Same target when at least one legal en-passant capture exists; otherwise INVALID.
    // Move generation and Zobrist hashing use this value.
    Square       legal_enpassant_target{INVALID};
    std::uint8_t halfmove_clock{};

    // Transition into this state.
    // Move recorded for this state; NULL_MOVE after FEN load or a null move.
    Move previous_move{NULL_MOVE};
    // Captured type removed by previous_move, or NO_PIECETYPE when none.
    PieceType captured_piece_type{NO_PIECETYPE};
};
