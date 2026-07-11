#include "board/board.hpp"

#include "core/attacks.hpp"
#include "core/square.hpp"
#include "movegen/movegen.hpp"
#include <algorithm>
#include <cassert>

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
    const Bitboard  to_bb     = bb::set(to);
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

            return (attacks::pawn_attacks(bb::set(from), turn) & to_bb) && target != NO_PIECE &&
                   color_of(target) == ~turn;
        }

        if (piecetype == KING)
            return attacks::piece_moves<KING>(from) & to_bb;

        return is_piece_type(piecetype) && piecetype != KING &&
               (attacks::piece_moves(from, piecetype, occupied) & to_bb);
    }

    case MOVE_PROM: {
        if (piecetype != PAWN || !valid_promotion_piece(mv.prom_piece()))
            return false;
        if (square::relative_rank(from, turn) != RANK7 || square::relative_rank(to, turn) != RANK8)
            return false;

        const int push_delta = (turn == WHITE) ? square::north : square::south;
        if (int(to) - int(from) == push_delta)
            return target == NO_PIECE;

        return (attacks::pawn_attacks(bb::set(from), turn) & to_bb) && target != NO_PIECE &&
               color_of(target) == ~turn;
    }

    case MOVE_EP: {
        if (piecetype != PAWN || to != enpassant_sq() || target != NO_PIECE)
            return false;
        if (!(attacks::pawn_attacks(bb::set(from), turn) & to_bb))
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
        (blockers(turn) & bb::set(from)))
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
        if (mv.type() != MOVE_CASTLE)
            return !attacks_to(to, ~turn, occupancy() ^ bb::set(from, to));
        return true;
    }

    // enpassant: check if captured pawn is blocking check
    else if (mv.type() == MOVE_EP) {
        Square   pawn = to + (turn == WHITE ? square::south : square::north);
        Bitboard occ  = (occupancy() ^ bb::set(from, pawn)) | bb::set(to);
        return !(pieces<BISHOP, QUEEN>(~turn) & attacks::piece_moves<BISHOP>(king, occ)) &&
               !(pieces<ROOK, QUEEN>(~turn) & attacks::piece_moves<ROOK>(king, occ));
    }

    // non-king moves must resolve any current check
    Bitboard checkers_bb = checkers();
    if (checkers_bb) {
        if (bb::is_many(checkers_bb))
            return false;

        Square checker = bb::lsb(checkers_bb);
        if (to != checker && !(square::between(king, checker) & bb::set(to)))
            return false;
    }

    // check if moved piece is pinned or moving in-line with check
    Bitboard pins = blockers(turn);
    return (!pins || !(pins & bb::set(from)) || (square::collinear(from, to) & bb::set(king)));
}

// Determine if a move gives check for the current board
bool Board::is_checking_move(Move mv) const {
    Square      from     = mv.from();
    Square      to       = mv.to();
    Color       opp      = ~turn;
    Square      opp_king = king_sq(opp);
    const auto& state    = position_state();

    // check if piece directly attacks the king or was a blocker
    const PieceType piecetype = type_of(piece_on(from));
    if (state.checking_squares(piecetype) & bb::set(to))
        return true;
    if ((state.blockers[opp] & bb::set(from)) && !(square::collinear(from, to) & bb::set(opp_king)))
        return true;

    switch (mv.type()) {
    case MOVE_PROM: {
        // check if a promotion attacks the enemy king
        Bitboard occupied = occupancy() ^ bb::set(from);
        return attacks::piece_moves(to, mv.prom_piece(), occupied) & bb::set(opp_king);
    }

    case MOVE_EP: {
        // check if captured pawn was blocking enemy king from attack
        Square   pawn     = to + (turn == WHITE ? square::south : square::north);
        Bitboard occupied = (occupancy() ^ bb::set(from, pawn)) | bb::set(to);
        return ((pieces<BISHOP, QUEEN>(turn) & attacks::piece_moves<BISHOP>(opp_king, occupied)) ||
                (pieces<ROOK, QUEEN>(turn) & attacks::piece_moves<ROOK>(opp_king, occupied)));
    }

    case MOVE_CASTLE: {
        // check if rook attacks enemy king
        Square   rook_from = castle::rook_from[to < from][turn];
        Square   rook_to   = castle::rook_to[to < from][turn];
        Bitboard occupied  = occupancy() ^ bb::set(from, rook_from, to, rook_to);
        return attacks::piece_moves<ROOK>(rook_to, occupied) & bb::set(opp_king);
    }

    case BASIC_MOVE: return false;
    default:         return false;
    }
}

void Board::make(Move move, PositionState& next_state) {
    assert(&next_state != active_position_state);
    position_key_history.push(key());

    const Square from           = move.from();
    const Square to             = move.to();
    const Square enpassant      = legal_enpassant_sq();
    const auto   piecetype      = piecetype_on(from);
    const auto   movetype       = move.type();
    const Color  opp            = ~turn;
    Square       capt_sq        = to;
    PieceType    capt_piecetype = captured_piece_type(move);

    if (movetype == MOVE_EP) {
        capt_sq          = to + (turn == WHITE ? square::south : square::north);
        squares[capt_sq] = NO_PIECE;
    }

    // create new state and update counters
    next_state            = PositionState(active_state(), move);
    active_position_state = &next_state;
    ++game_ply;
    auto& state = this->active_state();
    ++fullmove_clk;

    // handle piece capture
    state.captured = capt_piecetype;
    if (capt_piecetype != NO_PIECETYPE) {
        state.halfmove_clk = 0;
        remove_piece<true>(capt_sq, opp, capt_piecetype);
        if (can_castle(opp) && capt_piecetype == ROOK)
            disable_castle(opp, capt_sq);
    }

    if (enpassant != INVALID)
        state.zkey ^= zob::hash_ep(enpassant);

    // move the piece / castle
    move_piece<true>(from, to, turn, piecetype);
    if (movetype == MOVE_CASTLE) {
        auto   castle_side = CastleSide(to < from);
        Square rook_from   = castle::rook_from[castle_side][turn];
        Square rook_to     = castle::rook_to[castle_side][turn];
        move_piece<true>(rook_from, rook_to, turn, ROOK);
    }

    switch (piecetype) {
    case PAWN:
        // set enpassant square,
        state.halfmove_clk = 0;
        if (std::abs(to - from) == PAWN_PUSH2) {
            state.enpassant = to + (turn == WHITE ? square::south : square::north);
        } else if (movetype == MOVE_PROM) {
            remove_piece<true>(to, turn, PAWN);
            add_piece<true>(to, turn, move.prom_piece());
        }
        break;

    case KING:
        king_square[turn] = to;
        if (can_castle(turn))
            disable_castle(turn);
        break;

    case ROOK:
        if (can_castle(turn))
            disable_castle(turn, from);
        break;

    default: break;
    }

    turn        = opp;
    state.zkey ^= zob::turn;

    if (legal_enpassant_sq() != INVALID)
        state.zkey ^= zob::hash_ep(state.enpassant);

    update_check_data();
}

void Board::unmake(PositionState& prior_state) {
    assert(game_ply > 0);
    assert(&prior_state != active_position_state);

    const Color  opp      = turn;
    const Move   move     = active_state().previous_move;
    const Square from     = move.from();
    const Square to       = move.to();
    const auto   movetype = move.type();

    auto piecetype      = piecetype_on(to);
    auto capt_piecetype = active_state().captured;

    // decrement counters and pop state
    position_key_history.pop(prior_state.zkey);
    active_position_state = &prior_state;
    --game_ply;
    fullmove_clk--;
    turn = ~turn;

    // replace promoted piece with pawn
    if (movetype == MOVE_PROM) {
        remove_piece<false>(to, turn, piecetype);
        add_piece<false>(to, turn, PAWN);
        piecetype = PAWN;
    }

    // move the piece back, restore captured piece
    move_piece<false>(to, from, turn, piecetype);
    if (movetype == MOVE_CASTLE) {
        auto   castle_side = CastleSide(to < from);
        Square rook_from   = castle::rook_from[castle_side][turn];
        Square rook_to     = castle::rook_to[castle_side][turn];
        move_piece<false>(rook_to, rook_from, turn, ROOK);
    } else if (capt_piecetype != NO_PIECETYPE) {
        Square capt_sq =
            (movetype != MOVE_EP) ? to : to + (turn == WHITE ? square::south : square::north);
        add_piece<false>(capt_sq, opp, capt_piecetype);
    }

    if (piecetype == KING)
        king_square[turn] = from;
}

void Board::make_null(PositionState& next_state) {
    assert(&next_state != active_position_state);
    position_key_history.push(key());

    const Square hashed_enpassant = legal_enpassant_sq();

    next_state            = PositionState(active_state(), Move());
    active_position_state = &next_state;
    ++game_ply;
    auto& state = this->active_state();

    turn        = ~turn;
    state.zkey ^= zob::turn;
    if (hashed_enpassant != INVALID)
        state.zkey ^= zob::hash_ep(hashed_enpassant);

    update_check_data();
}

void Board::unmake_null(PositionState& prior_state) {
    assert(game_ply > 0);
    assert(&prior_state != active_position_state);

    position_key_history.pop(prior_state.zkey);
    active_position_state = &prior_state;
    --game_ply;
    turn = ~turn;
}

// stalemate from no legal moves
bool Board::is_stalemate() const {
    auto movelist = movegen::generate_pseudo_legal(*this);
    for (auto& move : movelist) {
        if (is_legal_generated_move(move))
            return false;
    }
    return true;
}

// draws from 50-move rule + 3-fold repetition
bool Board::is_draw(int search_ply) const {
    if (halfmove() >= 100)
        return true;

    const int rewind      = std::min<int>(halfmove(), position_key_history.count());
    int       repetitions = 0;

    for (int distance = 2; distance <= rewind; distance += 2) {
        const int index = position_key_history.count() - distance;
        if (index < 0)
            break;

        if (position_key_history[index] != key())
            continue;

        if (distance < search_ply)
            return true;
        if (++repetitions == 2)
            return true;
    }

    return false;
}

// convert a move to standard algebraic notation
std::string Board::toSAN(Move move) const {
    std::string result    = "";
    Square      from      = move.from();
    Square      to        = move.to();
    PieceType   piecetype = piecetype_on(from);

    if (move.type() == MOVE_CASTLE) {
        if (move.from() < move.to())
            return "O-O";
        else
            return "O-O-O";
    }

    if (piecetype != PAWN)
        result += std::toupper(to_char(piecetype));

    // handle move disambiguation
    auto movelist = movegen::generate_pseudo_legal(*this);
    for (auto& m : movelist) {
        bool ambiguous =
            (piecetype_on(m.from()) == piecetype) && (m.to() == to) && (m.from() != from);
        if (ambiguous && is_legal_generated_move(m)) {
            if (square::file_of(m.from()) != square::file_of(from))
                result += to_char(square::file_of(from));
            if (square::rank_of(m.from()) != square::rank_of(from))
                result += to_char(square::rank_of(from));
            break;
        }
    }

    if (is_capture(move)) {
        if (piecetype == PAWN)
            result += to_char(square::file_of(from));
        result += 'x';
    }

    result += to_string(to);
    if (move.type() == MOVE_PROM)
        result += '=' + to_char(move.prom_piece());
    if (is_checking_move(move))
        result += '+';

    return result;
}
