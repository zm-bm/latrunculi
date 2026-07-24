#include "board/board.hpp"

#include "core/attacks.hpp"
#include "core/move_geometry.hpp"
#include "eval/eval.hpp"

#include <algorithm>

namespace {

constexpr PieceType see_attacker_order[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

EvalValue see_initial_gain(const Board& board, Move move) noexcept {
    EvalValue gain = eval::piece(board.captured_piece_type(move)).mg;
    if (move.type() == MOVE_PROM)
        gain += eval::piece(move.prom_piece()).mg - eval::piece(PAWN).mg;

    return gain;
}

} // namespace

// Static exchange evaluation. Returns the likely material gain after a sequence
// of least-valuable recaptures on move.to().
EvalValue Board::see(Move move) const noexcept {
    const Square from = move.from();
    const Square to   = move.to();

    Color     side       = side_to_move();
    PieceType piece_type = move.type() == MOVE_PROM ? move.prom_piece() : piece_type_on(from);
    Bitboard  occupancy  = this->occupancy();
    Bitboard  from_bb    = bb::set(from);

    // Play the capture on an occupancy bitboard, including en passant's off-target pawn.
    if (move.type() == MOVE_EP) {
        const Square captured_square = move_geometry::enpassant_captured_square(to, side);
        bb::remove(occupancy, captured_square);
        bb::add(occupancy, to);
    }

    // Exclude removed pieces so an attacker cannot be selected twice.
    Bitboard attackers = attacks_to(to, occupancy) & occupancy;

    const Bitboard bishop_sliders = pieces<BISHOP, QUEEN>();
    const Bitboard rook_sliders   = pieces<ROOK, QUEEN>();

    // Swap-list of best case material gain for each depth.
    EvalValue gain[32] = {};
    gain[0]            = see_initial_gain(*this, move);

    int depth = 0;
    do {
        depth++;
        side = ~side;

        gain[depth] = eval::piece(piece_type).mg - gain[depth - 1];
        if (std::max(-gain[depth - 1], gain[depth]) < 0)
            break;

        occupancy ^= from_bb;

        // Removing the recapturer can reveal x-ray bishop, rook, or queen attacks.
        attackers |= (attacks::piece_moves<BISHOP>(to, occupancy) & bishop_sliders) |
                     (attacks::piece_moves<ROOK>(to, occupancy) & rook_sliders);
        attackers &= occupancy;

        from_bb = 0;
        for (PieceType attacker_type : see_attacker_order) {
            Bitboard attacker_bb = attackers & piece_bb[side][attacker_type];
            if (!attacker_bb)
                continue;

            attacker_bb = bb::lsb_mask(attacker_bb);
            if (attacker_type == KING) {
                // A king cannot recapture onto a square still attacked by the opponent.
                const Bitboard kingless_occupancy = occupancy ^ attacker_bb;
                if (attacks_to(to, ~side, kingless_occupancy) & kingless_occupancy)
                    continue;
            }

            piece_type = attacker_type;
            from_bb    = attacker_bb;
            break;
        }
    } while (from_bb);

    // Negamax the swap-list to get the final exchange value.
    while (--depth)
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);

    return gain[0];
}
