#include "board/board.hpp"

#include "board/ply_state.hpp"
#include "core/move_geometry.hpp"

#include <cassert>
#include <cstdlib>

void Board::make(Move move, PlyState& next_state) {
    assert(&next_state != active_ply_state);
    key_history.push_back(key());

    const Square    from         = move.from();
    const Square    to           = move.to();
    const MoveType  type         = move.type();
    const Color     mover        = turn;
    const Color     opponent     = ~mover;
    const PieceType moving_piece = piecetype_on(from);
    const Square    capture_square =
        type == MOVE_EP ? move_geometry::enpassant_captured_square(to, mover) : to;
    const PieceType captured_piece           = type == MOVE_EP ? PAWN : piecetype_on(to);
    const Square    previous_legal_enpassant = legal_enpassant_target();

    next_state       = PlyState(active_state(), move);
    active_ply_state = &next_state;
    ++ply_from_root;
    auto& state = this->active_state();
    ++absolute_ply;

    state.captured = captured_piece;
    if (captured_piece != NO_PIECETYPE) {
        state.halfmove_clk = 0;
        remove_piece<true>(capture_square, opponent, captured_piece);
        if (can_castle(opponent) && captured_piece == ROOK)
            disable_castle(opponent, capture_square);
    }

    if (previous_legal_enpassant != INVALID)
        state.zkey ^= zob::hash_ep(previous_legal_enpassant);

    move_piece<true>(from, to, mover, moving_piece);
    if (type == MOVE_CASTLE) {
        const auto& castling = move_geometry::castling(move_geometry::castle_side(from, to), mover);
        move_piece<true>(castling.rook_from, castling.rook_to, mover, ROOK);
    }

    switch (moving_piece) {
    case PAWN:
        state.halfmove_clk = 0;
        if (std::abs(to - from) == pawn_delta::double_push) {
            state.enpassant_target = move_geometry::enpassant_target(to, mover);
        } else if (type == MOVE_PROM) {
            const PieceType promotion_piece = move.prom_piece();
            remove_piece<true>(to, mover, PAWN);
            add_piece<true>(to, mover, promotion_piece);
        }
        break;

    case KING:
        king_square[mover] = to;
        if (can_castle(mover))
            disable_castle(mover);
        break;

    case ROOK:
        if (can_castle(mover))
            disable_castle(mover, from);
        break;

    default: break;
    }

    turn        = opponent;
    state.zkey ^= zob::hash_turn();

    update_check_data();
    if (state.enpassant_target != INVALID) {
        update_legal_enpassant_target();
        if (state.legal_enpassant_target != INVALID)
            state.zkey ^= zob::hash_ep(state.legal_enpassant_target);
    }
}

void Board::unmake(PlyState& prior_state) {
    assert(ply_from_root > 0);
    assert(&prior_state != active_ply_state);

    const Move      move            = active_state().previous_move;
    const Square    from            = move.from();
    const Square    to              = move.to();
    const MoveType  type            = move.type();
    const Color     opponent        = turn;
    const Color     mover           = ~opponent;
    const PieceType destination     = piecetype_on(to);
    const PieceType moving_piece    = type == MOVE_PROM ? PAWN : destination;
    const PieceType captured_piece  = active_state().captured;
    const PieceType promotion_piece = type == MOVE_PROM ? destination : NO_PIECETYPE;
    const Square    capture_square =
        type == MOVE_EP ? move_geometry::enpassant_captured_square(to, mover) : to;

    assert(!key_history.empty() && key_history.back() == prior_state.zkey);
    key_history.pop_back();
    active_ply_state = &prior_state;
    --ply_from_root;
    --absolute_ply;
    turn = mover;

    if (promotion_piece != NO_PIECETYPE) {
        remove_piece<false>(to, mover, promotion_piece);
        add_piece<false>(to, mover, PAWN);
    }

    move_piece<false>(to, from, mover, moving_piece);
    if (type == MOVE_CASTLE) {
        const auto& castling = move_geometry::castling(move_geometry::castle_side(from, to), mover);
        move_piece<false>(castling.rook_to, castling.rook_from, mover, ROOK);
    } else if (captured_piece != NO_PIECETYPE) {
        add_piece<false>(capture_square, opponent, captured_piece);
    }

    if (moving_piece == KING)
        king_square[mover] = from;
}

void Board::make_null(PlyState& next_state) {
    assert(&next_state != active_ply_state);
    key_history.push_back(key());

    const Square previous_legal_enpassant = legal_enpassant_target();

    next_state       = PlyState(active_state(), Move());
    active_ply_state = &next_state;
    ++ply_from_root;
    auto& state = this->active_state();

    turn        = ~turn;
    state.zkey ^= zob::hash_turn();
    if (previous_legal_enpassant != INVALID)
        state.zkey ^= zob::hash_ep(previous_legal_enpassant);

    update_check_data();
}

void Board::unmake_null(PlyState& prior_state) {
    assert(ply_from_root > 0);
    assert(&prior_state != active_ply_state);

    assert(!key_history.empty() && key_history.back() == prior_state.zkey);
    key_history.pop_back();
    active_ply_state = &prior_state;
    --ply_from_root;
    turn = ~turn;
}
