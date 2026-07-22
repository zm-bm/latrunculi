#include "board/board.hpp"

#include "board/ply_state.hpp"

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

void Board::copy_root_from(const Board& source, PlyState& root_state) noexcept {
    assert(this != &source);
    assert(&root_state != &source.ply_state());
    static_assert(PositionKeyHistory::capacity >=
                  fifty_move_rule_halfmoves - 1 + engine::max_search_ply);

    const PlyState source_state = source.ply_state();
    const int      retained_history =
        source_state.halfmove_clk < fifty_move_rule_halfmoves
                 ? std::min<int>(source_state.halfmove_clk, source.position_key_history.count())
                 : 0;

    copy_array(source.piece_bb, piece_bb);
    copy_array(source.piece_counts, piece_counts);
    copy_array(source.squares, squares);
    copy_array(source.king_square, king_square);
    turn         = source.turn;
    fullmove_clk = source.fullmove_clk;
    material     = source.material;
    psq_bonus    = source.psq_bonus;
    game_ply     = source.game_ply;

    root_state = source_state;
    bind_ply_state(root_state);

    // Earlier keys cannot affect repetition after the last irreversible move.
    // A position already drawn by the fifty-move rule needs no repetition keys.
    position_key_history.clear();
    const int first = source.position_key_history.count() - retained_history;
    for (int index = first; index < source.position_key_history.count(); ++index)
        position_key_history.push(source.position_key_history[index]);

    assert(position_key_history.count() + engine::max_search_ply <=
           static_cast<int>(PositionKeyHistory::capacity));
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
    fullmove_clk       = 0;
    active_state()     = PlyState{};
    game_ply           = 0;
    position_key_history.clear();
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

    auto sq = legal_enpassant_sq();
    if (sq != INVALID)
        zkey ^= zob::hash_ep(sq);

    return zkey;
}
