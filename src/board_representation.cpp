#include "board.hpp"

void Board::load_board(const Board* other) {
    if (!other)
        return;

    load_fen(other->toFEN());
    state = other->state;
    ply   = other->ply;
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
    state              = {State()};
    ply                = 0;
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
