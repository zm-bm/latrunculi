#pragma once

#include <array>
#include <cassert>

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
    uint64_t checkers{};
    // blockers[c]: single blockers between king c and enemy sliders.
    uint64_t blockers[N_COLORS]{};
    // pinners[c]: sliders of color c pinning enemy blockers.
    uint64_t pinners[N_COLORS]{};
    // Squares where each concrete piece type would check the enemy king.
    uint64_t checks[piece_slots]{};

    uint64_t checking_squares(PieceType piece) const { return checks[piece_slot(piece)]; }

    void set_checking_squares(PieceType piece, uint64_t squares) {
        checks[piece_slot(piece)] = squares;
    }

    // Incremental key and compact undo data for the move that reached this ply.
    uint64_t     zkey{};
    Move         previous_move{NULL_MOVE};
    PieceType    captured{NO_PIECETYPE};
    CastleRights castle{NO_CASTLE};
    Square       enpassant{INVALID};
    uint8_t      halfmove_clk{};

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
