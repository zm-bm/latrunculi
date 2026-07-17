#include "board/board.hpp"

#include "board/ply_state.hpp"

#include <cassert>
#include <cstdlib>

void Board::make(Move move, PlyState& next_state) {
    assert(&next_state != active_ply_state);
    position_key_history.push(key());

    const Square from           = move.from();
    const Square to             = move.to();
    const Square enpassant      = legal_enpassant_sq();
    const auto   piecetype      = piecetype_on(from);
    const auto   movetype       = move.type();
    const Color  opp            = ~turn;
    Square       capt_sq        = to;
    PieceType    capt_piecetype = captured_piece_type(move);

    if (movetype == MOVE_EP)
        capt_sq = to + (turn == WHITE ? square::south : square::north);

    next_state       = PlyState(active_state(), move);
    active_ply_state = &next_state;
    ++game_ply;
    auto& state = this->active_state();
    ++fullmove_clk;

    state.captured = capt_piecetype;
    if (capt_piecetype != NO_PIECETYPE) {
        state.halfmove_clk = 0;
        remove_piece<true>(capt_sq, opp, capt_piecetype);
        if (can_castle(opp) && capt_piecetype == ROOK)
            disable_castle(opp, capt_sq);
    }

    if (enpassant != INVALID)
        state.zkey ^= zob::hash_ep(enpassant);

    move_piece<true>(from, to, turn, piecetype);
    if (movetype == MOVE_CASTLE) {
        auto   castle_side = CastleSide(to < from);
        Square rook_from   = castle::rook_from[castle_side][turn];
        Square rook_to     = castle::rook_to[castle_side][turn];
        move_piece<true>(rook_from, rook_to, turn, ROOK);
    }

    switch (piecetype) {
    case PAWN:
        state.halfmove_clk = 0;
        if (std::abs(to - from) == pawn_delta::double_push) {
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

void Board::unmake(PlyState& prior_state) {
    assert(game_ply > 0);
    assert(&prior_state != active_ply_state);

    const Color  opp      = turn;
    const Move   move     = active_state().previous_move;
    const Square from     = move.from();
    const Square to       = move.to();
    const auto   movetype = move.type();

    auto piecetype      = piecetype_on(to);
    auto capt_piecetype = active_state().captured;

    position_key_history.pop(prior_state.zkey);
    active_ply_state = &prior_state;
    --game_ply;
    fullmove_clk--;
    turn = ~turn;

    if (movetype == MOVE_PROM) {
        remove_piece<false>(to, turn, piecetype);
        add_piece<false>(to, turn, PAWN);
        piecetype = PAWN;
    }

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

void Board::make_null(PlyState& next_state) {
    assert(&next_state != active_ply_state);
    position_key_history.push(key());

    const Square hashed_enpassant = legal_enpassant_sq();

    next_state       = PlyState(active_state(), Move());
    active_ply_state = &next_state;
    ++game_ply;
    auto& state = this->active_state();

    turn        = ~turn;
    state.zkey ^= zob::turn;
    if (hashed_enpassant != INVALID)
        state.zkey ^= zob::hash_ep(hashed_enpassant);

    update_check_data();
}

void Board::unmake_null(PlyState& prior_state) {
    assert(game_ply > 0);
    assert(&prior_state != active_ply_state);

    position_key_history.pop(prior_state.zkey);
    active_ply_state = &prior_state;
    --game_ply;
    turn = ~turn;
}
