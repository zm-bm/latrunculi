#pragma once

#include <array>
#include <cassert>
#include <cstdint>

#include "board/castling.hpp"
#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"

/**
 * Per-ply board state owned by the caller and used by Board.
 *
 * Board owns the durable piece representation. This holds the active ply's
 * reversible state plus cached check data for fast legality/search.
 */
struct PositionState {
    PositionState() = default;

    // Refreshed after FEN load, make, and null moves.
    // Enemy pieces checking the side-to-move king.
    Bitboard checkers{};
    // blockers[c]: single blockers between king c and enemy sliders.
    Bitboard blockers[N_COLORS]{};
    // pinners[c]: sliders of color c pinning enemy blockers.
    Bitboard pinners[N_COLORS]{};
    // Squares where each concrete piece type would check the enemy king.
    Bitboard checks[piece_slots]{};

    Bitboard checking_squares(PieceType piece) const { return checks[piece_slot(piece)]; }

    void set_checking_squares(PieceType piece, Bitboard squares) {
        checks[piece_slot(piece)] = squares;
    }

    // Incremental key and compact undo data for the move that reached this ply.
    PositionKey  zkey{};
    Move         previous_move{NULL_MOVE};
    PieceType    captured{NO_PIECETYPE};
    CastleRights castle{NO_CASTLE};
    Square       enpassant{INVALID};
    std::uint8_t halfmove_clk{};

    PositionState(const PositionState& prior_state, Move move)
        : zkey(prior_state.zkey),
          previous_move(move),
          castle(prior_state.castle),
          halfmove_clk(prior_state.halfmove_clk + 1) {}
};

// Fixed state storage for search/perft. child(ply) is where make() writes the
// next ply; parent(ply) is what unmake() restores.
class PositionStateStack {
public:
    PositionState& root() noexcept { return stack[0]; }

    PositionState& child(int ply) noexcept {
        assert(ply >= 0 && ply < engine::max_search_ply);
        return stack[ply + 1];
    }

    PositionState& parent(int ply) noexcept {
        assert(ply > 0 && ply <= engine::max_search_ply);
        return stack[ply - 1];
    }

private:
    std::array<PositionState, engine::max_search_ply + 1> stack{};
};
