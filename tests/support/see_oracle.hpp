#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>

#include "board/board.hpp"
#include "board/see_values.hpp"

namespace board_test {

namespace see_oracle {

// Deliberately use a square array and recursive captures, independent of the
// production attack-set updates and threshold cutoffs.
struct Position {
    std::array<Piece, N_SQUARES> squares{};
    Bitboard occupancy{};

    bool attacks(Square from, Square to) const {
        const Piece piece = squares[from];
        const int df = square::file_of(to) - square::file_of(from);
        const int dr = square::rank_of(to) - square::rank_of(from);
        switch (type_of(piece)) {
            case PAWN:   return std::abs(df) == 1 && dr == (color_of(piece) == WHITE ? 1 : -1);
            case KNIGHT: return std::abs(df) * std::abs(dr) == 2;
            case KING:   return std::max(std::abs(df), std::abs(dr)) == 1;
            case BISHOP:
                if (std::abs(df) != std::abs(dr))
                    return false;
                break;
            case ROOK:
                if (df != 0 && dr != 0)
                    return false;
                break;
            case QUEEN:
                if (df != 0 && dr != 0 && std::abs(df) != std::abs(dr))
                    return false;
                break;
            default: return false;
        }
        return !(square::between(from, to) & occupancy);
    }

    Position capture(Square from, Square to) const {
        Position next = *this;
        next.squares[to] = squares[from];
        next.squares[from] = NO_PIECE;
        bb::remove(next.occupancy, from);
        bb::add(next.occupancy, to);
        return next;
    }

    bool attacked(Square to, Color side) const {
        for (Square from = A1; from < N_SQUARES; ++from)
            if (from != to
                && squares[from] != NO_PIECE
                && color_of(squares[from]) == side
                && attacks(from, to))
                return true;
        return false;
    }

    int reply(Square to, Color side) const {
        for (PieceType type : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            for (Square from = A1; from < N_SQUARES; ++from) {
                if (from == to || squares[from] != make_piece(side, type) || !attacks(from, to))
                    continue;
                const Position next = capture(from, to);
                if (type == KING && next.attacked(to, ~side))
                    continue;
                // Non-king pins are ignored; later pawn recaptures keep the pawn price.
                return std::max(0, see::piece_value(type_of(squares[to])) - next.reply(to, ~side));
            }
        }
        return 0;
    }
};

} // namespace see_oracle

// Numerical exchange value under the engine's fixed prices.
inline int exchange_oracle(const Board& board, Move move) {
    see_oracle::Position position;
    position.occupancy = board.occupancy();
    for (Square square = A1; square < N_SQUARES; ++square)
        position.squares[square] = board.piece_on(square);

    int gain = see::piece_value(board.captured_piece_type(move));
    if (move.type() == MOVE_EP) {
        const Square victim =
            move_geometry::enpassant_captured_square(move.to(), board.side_to_move());
        position.squares[victim] = NO_PIECE;
        bb::remove(position.occupancy, victim);
    }
    position = position.capture(move.from(), move.to());
    if (move.type() == MOVE_PROM) {
        gain += see::piece_value(move.prom_piece()) - see::piece_value(PAWN);
        position.squares[move.to()] = make_piece(board.side_to_move(), move.prom_piece());
    }
    return gain - position.reply(move.to(), ~board.side_to_move());
}

} // namespace board_test
