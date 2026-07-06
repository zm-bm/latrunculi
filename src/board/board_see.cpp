#include "board/board.hpp"

#include "eval/eval.hpp"

#include <algorithm>

namespace {

constexpr PieceType see_attacker_order[] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

constexpr int see_piece_value(PieceType piece) {
    return piece == NO_PIECETYPE ? 0 : eval::piece(piece).mg;
}

int see_initial_gain(const Board& board, Move move) {
    int gain = see_piece_value(board.captured_piece_type(move));
    if (move.type() == MOVE_PROM)
        gain += see_piece_value(move.prom_piece()) - see_piece_value(PAWN);

    return gain;
}

} // namespace

// Threshold static exchange evaluation. Returns true when the exchange after
// move is at least threshold.
bool Board::seeAtLeast(Move move, int threshold) const {
    // The fast threshold path is only used by move ordering for ordinary
    // captures with small negative margins. Keep broader API calls exact.
    if (move.type() != BASIC_MOVE || threshold > 0 || threshold < -PAWN_MG)
        return seeMove(move) >= threshold;

    const Square from = move.from();
    const Square to   = move.to();

    const Color     us          = side_to_move();
    const uint64_t  from_bb     = bb::set(from);
    const uint64_t  target_bb   = bb::set(to);
    const PieceType moved_piece = move.type() == MOVE_PROM ? move.prom_piece() : piecetype_on(from);

    // Threshold SEE can often resolve before walking the full recapture chain.
    int balance = see_initial_gain(*this, move) - threshold;
    if (balance < 0)
        return false;

    balance = see_piece_value(moved_piece) - balance;
    if (balance <= 0)
        return true;

    // Play the capture on an occupancy bitboard, including en passant's off-target pawn.
    uint64_t occupied = occupancy();
    if (move.type() == MOVE_EP) {
        const Square captured = to + (us == WHITE ? SOUTH : NORTH);
        occupied              = (occupied ^ bb::set(captured)) | target_bb;
    }
    occupied ^= from_bb;

    // Get all pieces which attack the target square. Mask with occupied so that
    // a removed piece cannot attack twice.
    uint64_t attackers = attacks_to(to, occupied) & occupied;

    const uint64_t bishop_sliders = pieces<BISHOP, QUEEN>();
    const uint64_t rook_sliders   = pieces<ROOK, QUEEN>();

    Color side   = ~us;
    bool  result = true;

    while (true) {
        attackers &= occupied;

        uint64_t  attacker_bb = 0;
        PieceType attacker    = NO_PIECETYPE;
        for (PieceType p : see_attacker_order) {
            uint64_t candidates = attackers & piece_bb[side][p];
            if (!candidates)
                continue;

            candidates &= -candidates;
            if (p == KING) {
                // A king cannot recapture onto a square still attacked by the opponent.
                const uint64_t kingless_occupied = occupied ^ candidates;
                if (attacks_to(to, ~side, kingless_occupied) & kingless_occupied)
                    continue;
            }

            attacker_bb = candidates;
            attacker    = p;
            break;
        }

        if (!attacker_bb)
            break;

        result    = !result;
        occupied ^= attacker_bb;

        // Removing the recapturer can reveal x-ray bishop, rook, or queen attacks.
        attackers |= (bb::moves<BISHOP>(to, occupied) & bishop_sliders) |
                     (bb::moves<ROOK>(to, occupied) & rook_sliders);

        balance = see_piece_value(attacker) - balance;
        if (balance < static_cast<int>(result))
            break;

        side = ~side;
    }

    return result;
}

// Static exchange evaluation. Returns the likely material gain after a sequence
// of least-valuable recaptures on move.to().
int Board::seeMove(Move move) const {
    const Square from = move.from();
    const Square to   = move.to();

    const uint64_t target_bb = bb::set(to);
    Color          side      = side_to_move();
    PieceType      piece     = move.type() == MOVE_PROM ? move.prom_piece() : piecetype_on(from);
    uint64_t       occupied  = occupancy();
    uint64_t       from_bb   = bb::set(from);

    // Play the capture on an occupancy bitboard, including en passant's off-target pawn.
    if (move.type() == MOVE_EP) {
        const Square captured = to + (side == WHITE ? SOUTH : NORTH);
        occupied              = (occupied ^ bb::set(captured)) | target_bb;
    }

    // Get all pieces which attack the target square. Mask with occupied so that
    // a removed piece cannot attack twice.
    uint64_t attackers = attacks_to(to, occupied) & occupied;

    const uint64_t bishop_sliders = pieces<BISHOP, QUEEN>();
    const uint64_t rook_sliders   = pieces<ROOK, QUEEN>();

    // Swap-list of best case material gain for each depth.
    int gain[32] = {};
    gain[0]      = see_initial_gain(*this, move);

    int depth = 0;
    do {
        depth++;
        side = ~side;

        gain[depth] = eval::piece(piece).mg - gain[depth - 1];
        if (std::max(-gain[depth - 1], gain[depth]) < 0)
            break;

        occupied ^= from_bb;

        // Removing the recapturer can reveal x-ray bishop, rook, or queen attacks.
        attackers |= (bb::moves<BISHOP>(to, occupied) & bishop_sliders) |
                     (bb::moves<ROOK>(to, occupied) & rook_sliders);
        attackers &= occupied;

        from_bb = 0;
        for (PieceType p : see_attacker_order) {
            uint64_t attacker_bb = attackers & piece_bb[side][p];
            if (!attacker_bb)
                continue;

            attacker_bb &= -attacker_bb; // get least significant bit
            if (p == KING) {
                // A king cannot recapture onto a square still attacked by the opponent.
                const uint64_t kingless_occupied = occupied ^ attacker_bb;
                if (attacks_to(to, ~side, kingless_occupied) & kingless_occupied)
                    continue;
            }

            piece   = p;
            from_bb = attacker_bb;
            break;
        }
    } while (from_bb);

    // Negamax the swap-list to get the final exchange value.
    while (--depth)
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);

    return gain[0];
}
