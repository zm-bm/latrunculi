#pragma once

#include <array>
#include <cassert>
#include <cstdint>

#include "board/castling_rights.hpp"
#include "core/bitboard.hpp"
#include "core/constants.hpp"
#include "core/move.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

/**
 * Per-ply board state owned by the caller and used by Board.
 *
 * Board owns the durable piece representation. This holds the active ply's
 * reversible state plus cached tactical data for fast legality/search.
 */
struct PlyState {
    // Tactical cache refreshed after FEN load, make, and null moves.
    // Enemy pieces checking the side-to-move king.
    Bitboard checkers{};
    // Single pieces between each king and opposing sliders, regardless of color.
    std::array<Bitboard, N_COLORS> blockers{};

    // Reversible position state.
    PositionKey  zkey{};
    CastleRights castle{NO_CASTLE};
    Square       enpassant_target{INVALID};
    Square       legal_enpassant_target{INVALID};
    std::uint8_t halfmove_clk{};

    // Undo data for the move that reached this ply.
    Move      previous_move{NULL_MOVE};
    PieceType captured{NO_PIECETYPE};
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
