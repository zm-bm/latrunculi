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
    std::copy_n(&source[0][0], Rows * Cols, &target[0][0]);
}

} // namespace

void Board::copy_root_from(const Board& source, PlyState& root_state) {
    assert(this != &source);
    assert(&root_state != &source.ply_state());

    auto root_history = source.key_history;
    root_history.reserve(root_history.size() + engine::max_search_ply + 1);

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
    key_history = std::move(root_history);
}

void Board::reset() noexcept {
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
    absolute_ply       = 0;
    active_state()     = PlyState{};
    ply_from_root      = 0;
    key_history.clear();
}

PositionKey Board::calculate_key() const noexcept {
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

    const Square enpassant_target = legal_enpassant_target();
    if (enpassant_target != INVALID)
        zkey ^= zob::hash_ep(enpassant_target);

    return zkey;
}
