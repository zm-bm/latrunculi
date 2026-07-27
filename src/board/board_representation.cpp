#include "board/board.hpp"

#include "board/ply_state.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace {

template <typename T, std::size_t N>
void copy_array(const T (&source)[N], T (&target)[N]) {
    std::copy_n(source, N, target);
}

template <typename T, std::size_t Rows, std::size_t Cols>
void copy_array(const T (&source)[Rows][Cols], T (&target)[Rows][Cols]) {
    for (std::size_t row = 0; row < Rows; ++row)
        copy_array(source[row], target[row]);
}

} // namespace

Board::Board(PlyState& root_state, std::string_view fen) {
    key_history.reserve(engine::max_search_ply + 1);
    bind_ply_state(root_state);
    load_fen(fen);
}

void Board::copy_root_from(const Board& source, PlyState& root_state) {
    assert(this != &source);
    assert(&root_state != &source.ply_state());

    // Finish the only allocating work before mutating the destination.
    auto copied_history = source.key_history;
    copied_history.reserve(copied_history.size() + engine::max_search_ply + 1);

    const PlyState source_state = source.ply_state();

    copy_array(source.piece_bb, piece_bb);
    copy_array(source.piece_counts, piece_counts);
    copy_array(source.squares, squares);
    copy_array(source.king_square, king_square);
    turn          = source.turn;
    absolute_ply  = source.absolute_ply;
    material      = source.material;
    psq_bonus     = source.psq_bonus;
    ply_from_root = 0;

    root_state = source_state;
    bind_ply_state(root_state);
    key_history = std::move(copied_history);
}

void Board::reset() noexcept {
    for (int color_index = 0; color_index < N_COLORS; ++color_index) {
        for (int piece_index = 0; piece_index < N_PIECETYPES; ++piece_index) {
            piece_bb[color_index][piece_index]     = 0;
            piece_counts[color_index][piece_index] = 0;
        }
    }
    for (int square_index = 0; square_index < N_SQUARES; ++square_index)
        squares[square_index] = NO_PIECE;

    material           = {0, 0};
    psq_bonus          = {0, 0};
    king_square[WHITE] = INVALID;
    king_square[BLACK] = INVALID;
    turn               = WHITE;
    absolute_ply       = 0;
    active_state()     = PlyState{};
    ply_from_root      = 0;
    key_history.clear();
}

PositionKey Board::recompute_key() const noexcept {
    PositionKey zkey = 0;

    for (auto sq = A1; sq != INVALID; ++sq) {
        auto piece = piece_on(sq);
        if (piece != NO_PIECE)
            zkey ^= zob::hash_piece(color_of(piece), type_of(piece), sq);
    }

    if (turn == BLACK)
        zkey ^= zob::hash_turn();
    if (has_castling_right(CASTLE_KINGSIDE, WHITE))
        zkey ^= zob::hash_castle(CASTLE_KINGSIDE, WHITE);
    if (has_castling_right(CASTLE_QUEENSIDE, WHITE))
        zkey ^= zob::hash_castle(CASTLE_QUEENSIDE, WHITE);
    if (has_castling_right(CASTLE_KINGSIDE, BLACK))
        zkey ^= zob::hash_castle(CASTLE_KINGSIDE, BLACK);
    if (has_castling_right(CASTLE_QUEENSIDE, BLACK))
        zkey ^= zob::hash_castle(CASTLE_QUEENSIDE, BLACK);

    const Square enpassant_target = legal_enpassant_target();
    if (enpassant_target != INVALID)
        zkey ^= zob::hash_ep(enpassant_target);

    return zkey;
}
