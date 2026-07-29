#include "board/board.hpp"

#include "board/ply_state.hpp"

#include <algorithm>
#include <cstddef>
#include <type_traits>

static_assert(std::is_nothrow_copy_constructible_v<PlyState>);
static_assert(std::is_nothrow_copy_assignable_v<PlyState>);

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

Board::Board(std::string_view fen) {
    ply_states.reserve(engine::max_search_ply + 1);
    ply_states.emplace_back();
    active_state = &ply_states.back();
    load_fen(fen);
}

Board::Board(const Board& source) {
    *this = source;
}

Board& Board::operator=(const Board& source) {
    if (this == &source)
        return *this;

    assert(!source.ply_states.empty());

    // Reserve enough room for the copied history and a complete search before
    // mutating the destination's observable position.
    ply_states.reserve(source.ply_states.size() + engine::max_search_ply);
    ply_states.assign(source.ply_states.begin(), source.ply_states.end());

    copy_array(source.piece_bb, piece_bb);
    copy_array(source.piece_counts, piece_counts);
    copy_array(source.squares, squares);
    copy_array(source.king_square, king_square);
    turn         = source.turn;
    absolute_ply = source.absolute_ply;
    material     = source.material;
    psq_bonus    = source.psq_bonus;

    active_state = &ply_states.back();
    return *this;
}

void Board::clear_position() noexcept {
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
    ply_states.resize(1);
    active_state = &ply_states.back();
    ply_state()  = PlyState{};
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
