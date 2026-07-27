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

    Color     side             = side_to_move();
    PieceType piece_type       = move.type() == MOVE_PROM ? move.prom_piece() : piece_type_on(from);
    Bitboard  occupancy        = this->occupancy();
    Bitboard  current_attacker = bb::set(from);

    // Play the capture on an occupancy bitboard, including en passant's off-target pawn.
    if (move.type() == MOVE_EP) {
        const Square captured_square = move_geometry::enpassant_captured_square(to, side);
        bb::remove(occupancy, captured_square);
        bb::add(occupancy, to);
    }

    // Exclude removed pieces so an attacker cannot be selected twice.
    Bitboard attackers = all_attackers_to(to, occupancy) & occupancy;

    const Bitboard bishop_sliders = pieces<BISHOP, QUEEN>();
    const Bitboard rook_sliders   = pieces<ROOK, QUEEN>();

    // The initial gain occupies index zero; later entries describe successive
    // recaptures. The parser's 32-piece limit bounds the list to 32 entries.
    EvalValue gains[32] = {};
    gains[0]            = see_initial_gain(*this, move);

    int depth = 0;
    do {
        ++depth;
        side = ~side;

        gains[depth] = eval::piece(piece_type).mg - gains[depth - 1];
        // If stopping and continuing are both losing for the side to move,
        // deeper recaptures cannot change the backed-up result.
        if (std::max(-gains[depth - 1], gains[depth]) < 0)
            break;

        occupancy ^= current_attacker;

        // Removing the recapturer can reveal x-ray bishop, rook, or queen attacks.
        attackers |= (attacks::piece_moves<BISHOP>(to, occupancy) & bishop_sliders)
                   | (attacks::piece_moves<ROOK>(to, occupancy) & rook_sliders);
        attackers &= occupancy;

        current_attacker = 0;
        for (PieceType attacker_type : see_attacker_order) {
            Bitboard candidate_attacker = attackers & piece_bb[side][attacker_type];
            if (!candidate_attacker)
                continue;

            candidate_attacker = bb::lsb_mask(candidate_attacker);
            if (attacker_type == KING) {
                // A king cannot recapture onto a square still attacked by the opponent.
                const Bitboard kingless_occupancy = occupancy ^ candidate_attacker;
                if (attacks_to(to, ~side, kingless_occupancy) & kingless_occupancy)
                    continue;
            }

            piece_type       = attacker_type;
            current_attacker = candidate_attacker;
            break;
        }
    } while (current_attacker);

    // Negamax the swap-list to get the final exchange value.
    while (--depth)
        gains[depth - 1] = -std::max(-gains[depth - 1], gains[depth]);

    return gains[0];
}
