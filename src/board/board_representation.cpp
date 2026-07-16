#include "board/board.hpp"

#include <algorithm>
#include <cstddef>

namespace {

template <typename T, std::size_t N>
void copy_array(const T (&source)[N], T (&target)[N]) {
    std::copy_n(source, N, target);
}

template <typename T, std::size_t Rows, std::size_t Cols>
void copy_array(const T (&source)[Rows][Cols], T (&target)[Rows][Cols]) {
    std::copy_n(&source[0][0], Rows * Cols, &target[0][0]);
}

} // namespace

void Board::load_board(const Board* other) {
    if (!other || other == this)
        return;

    copy_array(other->piece_bb, piece_bb);
    copy_array(other->piece_counts, piece_counts);
    copy_array(other->squares, squares);
    copy_array(other->king_square, king_square);
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
        for (int p = 0; p < N_PIECETYPES; ++p) {
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
    Bitboard    capturers = pieces<PAWN>(side) & attacks::pawn_attacks(enpassant, ~side);

    while (capturers) {
        const Square from = bb::lsb_pop(capturers);
        if (is_legal_move(Move(from, enpassant, MOVE_EP)))
            return enpassant;
    }

    return INVALID;
}

PositionKey Board::calculate_key() const {
    PositionKey zkey = 0;

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
