#pragma once

#include <algorithm>
#include <array>

#include "board/board.hpp"
#include "core/types.hpp"
#include "eval/parameters.hpp"
#include "eval/tapered_score.hpp"

namespace eval {

struct FinalizationContext {
    // Color-indexed internally; exported records use White, Black order.
    std::array<std::array<int, 5>, N_COLORS> pieces{};
    std::array<int, N_COLORS> pawn_flanks{};   // a-d = 1, e-h = 2
    std::array<int, N_COLORS> bishop_colors{}; // dark = 1, light = 2

    int count(Color c, PieceType p) const { return pieces[c][p - PAWN]; }

    int non_pawn_material(Color c) const {
        return count(c, KNIGHT) * knight.mg
            + count(c, BISHOP) * bishop.mg
            + count(c, ROOK) * rook.mg
            + count(c, QUEEN) * queen.mg;
    }

    static FinalizationContext from_board(const Board& board) {
        FinalizationContext result;
        for (Color c : {WHITE, BLACK}) {
            for (int p = PAWN; p <= QUEEN; ++p)
                result.pieces[c][p - PAWN] = board.count(c, PieceType(p));
            const Bitboard pawns = board.pieces<PAWN>(c);
            result.pawn_flanks[c] = int(bool(pawns & 0x0f0f0f0f0f0f0f0fULL))
                | (int(bool(pawns & 0xf0f0f0f0f0f0f0f0ULL)) << 1);
            const Bitboard bishops = board.pieces<BISHOP>(c);
            result.bishop_colors[c] = int(bool(bishops & masks::dark_squares))
                | (int(bool(bishops & masks::light_squares)) << 1);
        }
        return result;
    }
};

// These are conversion heuristics, not claims of a theoretical draw.
inline int conversion_target(const FinalizationContext& context, Color strong, int base) {
    int target = base;
    if (context.count(WHITE, BISHOP) == 1
        && context.count(BLACK, BISHOP) == 1
        && (context.bishop_colors[WHITE] ^ context.bishop_colors[BLACK]) == 3) {
        const auto& w = context.pieces[WHITE];
        const auto& b = context.pieces[BLACK];
        if (w[1] == b[1] && w[3] == b[3] && w[4] == 0 && b[4] == 0) {
            if (w[1] == 0 && w[3] == 0)
                target = std::min(target, 32);
            else if ((w[1] == 1 && w[3] == 0) || (w[1] == 0 && w[3] == 1))
                target = std::min(target, 48);
        }
    }
    const int weak_material = context.non_pawn_material(~strong);
    const int advantage = context.non_pawn_material(strong) - weak_material;
    if (context.count(strong, PAWN) == 0
        && weak_material > 0
        && advantage >= 0
        && advantage <= bishop.mg)
        target = std::min(target, 16);
    if (context.pawn_flanks[strong] == 1 || context.pawn_flanks[strong] == 2)
        target = std::min(target, base - 4);
    return target;
}

struct FinalizedScore {
    TaperedScore scaled;
    Score tapered;
    Score side_to_move;
    Score value;
    int base_scale;
    int target_scale;
    int effective_scale;
};

// Production evaluation and feature reconstruction supply their own material
// inputs; the integer conversion from a raw score is shared.
inline FinalizedScore finalize_score(
    TaperedScore phase_scores,
    const FinalizationContext& context,
    Color turn,
    int strength = endgame_scale_strength.eg) {
    const Color strong = phase_scores.eg < 0 ? BLACK : WHITE;
    const int base =
        std::min(scale_limit, scale_base + scale_per_pawn * context.count(strong, PAWN));
    const int target = conversion_target(context, strong, base);
    const int scale = base - (base - target) * strength / scale_limit;
    phase_scores.eg = (phase_scores.eg * scale) / scale_limit;

    const int material = std::clamp(
        context.non_pawn_material(WHITE) + context.non_pawn_material(BLACK),
        material_eg,
        material_mg);
    const int phase = ((material - material_eg) * phase_limit) / (material_mg - material_eg);
    const Score tapered =
        (phase_scores.mg * phase + phase_scores.eg * (phase_limit - phase)) / phase_limit;
    const Score side_to_move = tapered * (turn == WHITE ? 1 : -1);
    return {phase_scores, tapered, side_to_move, side_to_move + tempo_bonus, base, target, scale};
}

} // namespace eval
