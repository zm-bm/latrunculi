#include "board/board.hpp"

#include "board/see_values.hpp"
#include "core/attacks.hpp"
#include "core/move_geometry.hpp"

#include <cassert>

namespace {

constexpr PieceType see_attacker_order[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};
} // namespace

bool Board::see(Move move, int threshold) const noexcept {
    assert(is_pseudo_legal(move));
    assert(is_capture(move) || move.type() == MOVE_PROM);

    int initial_gain = see::piece_value(captured_piece_type(move));
    if (move.type() == MOVE_PROM)
        initial_gain += see::piece_value(move.prom_piece()) - see::piece_value(PAWN);
    if (initial_gain < threshold)
        return false;

    const Square    to   = move.to();
    Color           side = side_to_move();
    const PieceType moving_piece =
        move.type() == MOVE_PROM ? move.prom_piece() : piece_type_on(move.from());
    const int moving_value = see::piece_value(moving_piece);
    // Check both bounds before subtraction so even extreme thresholds are safe.
    if (threshold <= initial_gain - moving_value)
        return true;
    int balance = moving_value - initial_gain + threshold;

    Bitboard occupancy = this->occupancy();

    if (move.type() == MOVE_EP) {
        bb::remove(occupancy, move_geometry::enpassant_captured_square(to, side));
        bb::add(occupancy, to);
    }

    // Build the first attack set after the initial capture, including its x-rays.
    occupancy ^= bb::set(move.from());
    Bitboard       attackers      = all_attackers_to(to, occupancy) & occupancy;
    const Bitboard bishop_sliders = pieces<BISHOP, QUEEN>();
    const Bitboard rook_sliders   = pieces<ROOK, QUEEN>();
    bool           result         = true;

    // Non-king pins are ignored; later pawn recaptures keep their pawn price.
    for (;;) {
        side = ~side;

        Bitboard  recapturer      = 0;
        PieceType recapturer_type = NO_PIECETYPE;
        for (PieceType attacker_type : see_attacker_order) {
            Bitboard candidate = attackers & piece_bb[side][attacker_type];
            if (!candidate)
                continue;

            candidate = bb::lsb_mask(candidate);
            if (attacker_type == KING) {
                const Bitboard kingless_occupancy = occupancy ^ candidate;
                if (attacks_to(to, ~side, kingless_occupancy) & kingless_occupancy)
                    continue;
            }

            recapturer_type = attacker_type;
            recapturer      = candidate;
            break;
        }

        if (!recapturer)
            return result;
        result  = !result;
        balance = see::piece_value(recapturer_type) - balance;
        // Equality succeeds for the original side, but fails for its opponent.
        if (balance < static_cast<int>(result))
            return result;

        occupancy ^= recapturer;
        attackers |= (attacks::piece_moves<BISHOP>(to, occupancy) & bishop_sliders)
                   | (attacks::piece_moves<ROOK>(to, occupancy) & rook_sliders);
        attackers &= occupancy;
    }
}
