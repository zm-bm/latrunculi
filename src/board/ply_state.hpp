#pragma once

#include <array>
#include <cassert>
#include <cstdint>

#include "board/castling.hpp"
#include "core/bitboard.hpp"
#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

struct TacticalState {
    // Enemy pieces checking the side-to-move king.
    Bitboard checkers{};
    // blockers[c]: single blockers between king c and enemy sliders.
    std::array<Bitboard, N_COLORS> blockers{};
    // pinners[c]: sliders of color c pinning enemy blockers.
    std::array<Bitboard, N_COLORS> pinners{};
    // Squares where each concrete piece type would check the enemy king.
    std::array<Bitboard, piece_slots> checks{};

    [[nodiscard]] Bitboard checking_squares(PieceType piece) const noexcept {
        return checks[piece_slot(piece)];
    }
};

/**
 * Per-ply board state owned by the caller and used by Board.
 *
 * Board owns the durable piece representation. This holds the active ply's
 * reversible state plus cached check data for fast legality/search.
 */
struct PlyState {
    PlyState() = default;

    // Refreshed after FEN load, make, and null moves.
    TacticalState tactical{};

    // Incremental key and compact undo data for the move that reached this ply.
    PositionKey  zkey{};
    Move         previous_move{NULL_MOVE};
    PieceType    captured{NO_PIECETYPE};
    CastleRights castle{NO_CASTLE};
    Square       enpassant{INVALID};
    std::uint8_t halfmove_clk{};

    PlyState(const PlyState& prior_state, Move move)
        : zkey(prior_state.zkey),
          previous_move(move),
          castle(prior_state.castle),
          halfmove_clk(prior_state.halfmove_clk + 1) {}
};

// Fixed state storage for search/perft. child(ply) is where make() writes the
// next ply; parent(ply) is what unmake() restores.
class PlyStateStack {
public:
    [[nodiscard]] PlyState& root() noexcept { return stack[0]; }

    [[nodiscard]] PlyState& child(int ply) noexcept {
        assert(ply >= 0 && ply < engine::max_search_ply);
        return stack[ply + 1];
    }

    [[nodiscard]] PlyState& parent(int ply) noexcept {
        assert(ply > 0 && ply <= engine::max_search_ply);
        return stack[ply - 1];
    }

private:
    std::array<PlyState, engine::max_search_ply + 1> stack{};
};
