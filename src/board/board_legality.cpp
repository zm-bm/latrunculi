#include "board/board.hpp"

#include "core/attacks.hpp"
#include "core/square.hpp"

namespace {

[[gnu::always_inline]] inline void
calculate_pins(const Board& board, Color color, TacticalState& tactical) {
    const Color  opponent = ~color;
    const Square king     = board.king_sq(color);
    Bitboard     snipers =
        (attacks::piece_moves<BISHOP>(king) & board.pieces<BISHOP, QUEEN>(opponent)) |
        (attacks::piece_moves<ROOK>(king) & board.pieces<ROOK, QUEEN>(opponent));
    const Bitboard occupancy = board.occupancy() ^ snipers;

    tactical.blockers[color]   = 0;
    tactical.pinners[opponent] = 0;

    while (snipers) {
        const Square   pinner         = bb::lsb_pop(snipers);
        const Bitboard pieces_between = occupancy & square::between(king, pinner);

        if (bb::is_one(pieces_between)) {
            tactical.blockers[color] |= pieces_between;
            if (pieces_between & board.pieces(color))
                bb::add(tactical.pinners[opponent], pinner);
        }
    }
}

[[gnu::always_inline]] inline void calculate_tactical_state(const Board&   board,
                                                            TacticalState& tactical) {
    const Color    opponent      = ~board.side_to_move();
    const Square   opponent_king = board.king_sq(opponent);
    const Bitboard occupancy     = board.occupancy();

    tactical.checkers = board.attacks_to(board.king_sq(board.side_to_move()), opponent);
    tactical.checks[piece_slot(PAWN)]   = attacks::pawn_attacks(opponent_king, opponent);
    tactical.checks[piece_slot(KNIGHT)] = attacks::piece_moves<KNIGHT>(opponent_king, occupancy);
    tactical.checks[piece_slot(BISHOP)] = attacks::piece_moves<BISHOP>(opponent_king, occupancy);
    tactical.checks[piece_slot(ROOK)]   = attacks::piece_moves<ROOK>(opponent_king, occupancy);
    tactical.checks[piece_slot(QUEEN)] =
        tactical.checking_squares(BISHOP) | tactical.checking_squares(ROOK);
    tactical.checks[piece_slot(KING)] = 0;

    calculate_pins(board, WHITE, tactical);
    calculate_pins(board, BLACK, tactical);
}

} // namespace

void Board::update_check_data() {
    calculate_tactical_state(*this, active_state().tactical);
}

Square Board::legal_enpassant_sq() const {
    const Square enpassant = enpassant_sq();
    if (enpassant == INVALID)
        return INVALID;

    const Color side      = side_to_move();
    Bitboard    capturers = pieces<PAWN>(side) & attacks::pawn_attacks(enpassant, ~side);

    while (capturers) {
        const Square from = bb::lsb_pop(capturers);
        if (is_legal_move(Move(from, enpassant, MOVE_EP)))
            return enpassant;
    }

    return INVALID;
}

bool Board::is_pseudo_legal(Move mv) const {
    if (mv.is_null())
        return false;

    const Square from = mv.from();
    const Square to   = mv.to();
    if (from == to)
        return false;

    const Piece piece = piece_on(from);
    if (piece == NO_PIECE || color_of(piece) != turn)
        return false;

    const Piece target = piece_on(to);
    if (target != NO_PIECE && color_of(target) == turn)
        return false;

    const PieceType piecetype = type_of(piece);
    const Bitboard  occupied  = occupancy();

    switch (mv.type()) {
    case BASIC_MOVE: {
        if (piecetype == PAWN) {
            const int push_delta = (turn == WHITE) ? square::north : square::south;
            const int move_delta = int(to) - int(from);

            if (square::relative_rank(to, turn) == RANK8)
                return false;

            if (move_delta == push_delta)
                return target == NO_PIECE;

            if (move_delta == 2 * push_delta) {
                const Square mid = from + push_delta;
                return square::relative_rank(from, turn) == RANK2 && target == NO_PIECE &&
                       piece_on(mid) == NO_PIECE;
            }

            return bb::contains(attacks::pawn_attacks(from, turn), to) && target != NO_PIECE &&
                   color_of(target) == ~turn;
        }

        if (piecetype == KING)
            return bb::contains(attacks::piece_moves<KING>(from), to);

        return is_piece_type(piecetype) && piecetype != KING &&
               bb::contains(attacks::piece_moves(from, piecetype, occupied), to);
    }

    case MOVE_PROM: {
        if (piecetype != PAWN || !valid_promotion_piece(mv.prom_piece()))
            return false;
        if (square::relative_rank(from, turn) != RANK7 || square::relative_rank(to, turn) != RANK8)
            return false;

        const int push_delta = (turn == WHITE) ? square::north : square::south;
        if (int(to) - int(from) == push_delta)
            return target == NO_PIECE;

        return bb::contains(attacks::pawn_attacks(from, turn), to) && target != NO_PIECE &&
               color_of(target) == ~turn;
    }

    case MOVE_EP: {
        if (piecetype != PAWN || to != enpassant_sq() || target != NO_PIECE)
            return false;
        if (!bb::contains(attacks::pawn_attacks(from, turn), to))
            return false;

        const Square captured = to + (turn == WHITE ? square::south : square::north);
        return piece_on(captured) == make_piece(~turn, PAWN);
    }

    case MOVE_CASTLE: {
        if (piecetype != KING)
            return false;

        const Square expected_from = (turn == WHITE) ? E1 : E8;
        if (from != expected_from)
            return false;

        const CastleSide side = (to > from) ? CASTLE_KINGSIDE : CASTLE_QUEENSIDE;
        const Square     expected_to =
            (side == CASTLE_KINGSIDE) ? ((turn == WHITE) ? G1 : G8) : ((turn == WHITE) ? C1 : C8);
        if (to != expected_to)
            return false;

        if (side == CASTLE_KINGSIDE) {
            if (!can_castle_kingside(turn))
                return false;
        } else if (!can_castle_queenside(turn)) {
            return false;
        }

        if (piece_on(castle::rook_from[side][turn]) != make_piece(turn, ROOK))
            return false;
        if (occupied & castle::path[side][turn])
            return false;

        return !attacks_to(castle::kingpath[side][turn], ~turn);
    }
    }

    return false;
}

bool Board::is_legal_move(Move mv) const {
    return is_pseudo_legal(mv) && is_legal_pseudo_move(mv);
}

// Requires a generated pseudo-legal move. Only strict cases can expose or leave king in check.
bool Board::is_legal_generated_move(Move mv) const {
    const Square from = mv.from();

    if (checkers() || from == king_sq(turn) || mv.type() == MOVE_EP ||
        bb::contains(blockers(turn), from))
        return is_legal_pseudo_move(mv);

    return true;
}

// Requires a pseudo-legal move. Filters moves that expose or leave the king in check.
bool Board::is_legal_pseudo_move(Move mv) const {
    Square from = mv.from();
    Square to   = mv.to();
    Square king = king_sq(turn);

    // king move: check if king is attacked(castle legality handled in movegen)
    if (from == king) {
        if (mv.type() != MOVE_CASTLE) {
            Bitboard occupied = occupancy();
            bb::move(occupied, from, to);
            return !attacks_to(to, ~turn, occupied);
        }
        return true;
    }

    // enpassant: check if captured pawn is blocking check
    else if (mv.type() == MOVE_EP) {
        Square   pawn = to + (turn == WHITE ? square::south : square::north);
        Bitboard occ  = occupancy();
        bb::move(occ, from, to);
        bb::remove(occ, pawn);
        return !(pieces<BISHOP, QUEEN>(~turn) & attacks::piece_moves<BISHOP>(king, occ)) &&
               !(pieces<ROOK, QUEEN>(~turn) & attacks::piece_moves<ROOK>(king, occ));
    }

    // non-king moves must resolve any current check
    Bitboard checkers_bb = checkers();
    if (checkers_bb) {
        if (bb::is_many(checkers_bb))
            return false;

        Square checker = bb::lsb(checkers_bb);
        if (to != checker && !bb::contains(square::between(king, checker), to))
            return false;
    }

    // check if moved piece is pinned or moving in-line with check
    Bitboard pins = blockers(turn);
    return (!pins || !bb::contains(pins, from) || bb::contains(square::collinear(from, to), king));
}

// Determine if a move gives check for the current board
bool Board::is_checking_move(Move mv) const {
    Square      from     = mv.from();
    Square      to       = mv.to();
    Color       opp      = ~turn;
    Square      opp_king = king_sq(opp);
    const auto& state    = ply_state();

    // check if piece directly attacks the king or was a blocker
    const PieceType piecetype = type_of(piece_on(from));
    if (bb::contains(state.tactical.checking_squares(piecetype), to))
        return true;
    if (bb::contains(state.tactical.blockers[opp], from) &&
        !bb::contains(square::collinear(from, to), opp_king))
        return true;

    switch (mv.type()) {
    case MOVE_PROM: {
        // check if a promotion attacks the enemy king
        Bitboard occupied = occupancy();
        bb::remove(occupied, from);
        return bb::contains(attacks::piece_moves(to, mv.prom_piece(), occupied), opp_king);
    }

    case MOVE_EP: {
        // check if captured pawn was blocking enemy king from attack
        Square   pawn     = to + (turn == WHITE ? square::south : square::north);
        Bitboard occupied = occupancy();
        bb::move(occupied, from, to);
        bb::remove(occupied, pawn);
        return ((pieces<BISHOP, QUEEN>(turn) & attacks::piece_moves<BISHOP>(opp_king, occupied)) ||
                (pieces<ROOK, QUEEN>(turn) & attacks::piece_moves<ROOK>(opp_king, occupied)));
    }

    case MOVE_CASTLE: {
        // check if rook attacks enemy king
        Square   rook_from = castle::rook_from[to < from][turn];
        Square   rook_to   = castle::rook_to[to < from][turn];
        Bitboard occupied  = occupancy();
        bb::move(occupied, from, to);
        bb::move(occupied, rook_from, rook_to);
        return bb::contains(attacks::piece_moves<ROOK>(rook_to, occupied), opp_king);
    }

    case BASIC_MOVE: return false;
    default:         return false;
    }
}
