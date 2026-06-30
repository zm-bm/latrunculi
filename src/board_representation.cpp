#include "board.hpp"

#include <algorithm>

void Board::load_board(const Board* other) {
    if (!other || other == this)
        return;

    const auto piece_slots = static_cast<size_t>(N_COLORS) * static_cast<size_t>(N_PIECES);
    std::copy(&other->piece_bb[0][0], &other->piece_bb[0][0] + piece_slots, &piece_bb[0][0]);
    std::copy(
        &other->piece_counts[0][0], &other->piece_counts[0][0] + piece_slots, &piece_counts[0][0]);
    std::copy(other->squares, other->squares + N_SQUARES, squares);
    std::copy(other->king_square, other->king_square + N_COLORS, king_square);
    turn                 = other->turn;
    fullmove_clk         = other->fullmove_clk;
    material             = other->material;
    psq_bonus            = other->psq_bonus;
    game_ply             = other->game_ply;
    position_key_history = other->position_key_history;
    active_state()       = other->position_state();
}

void Board::reset() {
    for (int c = 0; c < N_COLORS; ++c) {
        for (int p = 0; p < N_PIECES; ++p) {
            piece_bb[c][p]     = 0;
            piece_counts[c][p] = 0;
        }
    }
    for (int sq = 0; sq < N_SQUARES; ++sq)
        squares[sq] = NO_PIECE;

    material           = {0, 0};
    psq_bonus          = {0, 0};
    king_square[WHITE] = INVALID;
    king_square[BLACK] = INVALID;
    turn               = WHITE;
    fullmove_clk       = 0;
    active_state()     = PositionState{};
    game_ply           = 0;
    position_key_history.clear();
}

Square Board::legal_enpassant_sq() const {
    const Square enpassant = enpassant_sq();
    if (enpassant == INVALID)
        return INVALID;

    const Color side      = side_to_move();
    uint64_t    capturers = pieces<PAWN>(side) & bb::pawn_attacks(bb::set(enpassant), ~side);

    while (capturers) {
        const Square from = bb::lsb_pop(capturers);
        if (is_legal_move(Move(from, enpassant, MOVE_EP)))
            return enpassant;
    }

    return INVALID;
}

uint64_t Board::calculate_key() const {
    uint64_t zkey = 0;

    for (auto sq = A1; sq != INVALID; ++sq) {
        auto piece = piece_on(sq);
        if (piece != NO_PIECE)
            zkey ^= zob::hash_piece(color_of(piece), type_of(piece), sq);
    }

    if (turn == BLACK)
        zkey ^= zob::turn;
    if (can_castle_kingside(WHITE))
        zkey ^= zob::castle[CASTLE_KINGSIDE][WHITE];
    if (can_castle_queenside(WHITE))
        zkey ^= zob::castle[CASTLE_QUEENSIDE][WHITE];
    if (can_castle_kingside(BLACK))
        zkey ^= zob::castle[CASTLE_KINGSIDE][BLACK];
    if (can_castle_queenside(BLACK))
        zkey ^= zob::castle[CASTLE_QUEENSIDE][BLACK];

    auto sq = legal_enpassant_sq();
    if (sq != INVALID)
        zkey ^= zob::hash_ep(sq);

    return zkey;
}
