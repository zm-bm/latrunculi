#pragma once

#include "core/piece.hpp"
#include "core/square.hpp"
#include "eval/parameters.hpp"
#include "eval/tapered_score.hpp"

namespace eval {

// Board-owned, incrementally maintained HCE terms. This is evaluation state,
// not an intrinsic property of the chess position representation.
class BaseTerms {
public:
    [[nodiscard]] constexpr TaperedScore material() const noexcept { return material_score; }
    [[nodiscard]] constexpr TaperedScore piece_square() const noexcept {
        return piece_square_score;
    }

    constexpr void add_piece(PieceType piece_type, Color color, Square square) noexcept {
        material_score += eval::piece(piece_type, color);
        piece_square_score += eval::piece_sq(piece_type, color, square);
    }

    constexpr void remove_piece(PieceType piece_type, Color color, Square square) noexcept {
        material_score -= eval::piece(piece_type, color);
        piece_square_score -= eval::piece_sq(piece_type, color, square);
    }

    constexpr void move_piece(PieceType piece_type, Color color, Square from, Square to) noexcept {
        piece_square_score +=
            eval::piece_sq(piece_type, color, to) - eval::piece_sq(piece_type, color, from);
    }

    friend constexpr bool operator==(const BaseTerms&, const BaseTerms&) noexcept = default;

private:
    TaperedScore material_score     = TaperedScore::Zero;
    TaperedScore piece_square_score = TaperedScore::Zero;
};

} // namespace eval
