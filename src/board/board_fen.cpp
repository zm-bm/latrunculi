#include "board/board.hpp"

#include "board/fen.hpp"

#include <sstream>

void Board::load_fen(const std::string& fen) {
    FenParser parser(fen);
    reset();

    for (const auto p : parser.pieces) {
        add_piece<true>(p.square, p.color, p.type);
        if (p.type == KING)
            king_square[p.color] = p.square;
    }

    auto& state        = this->active_state();
    turn               = parser.turn;
    state.castle       = parser.castle;
    state.enpassant    = parser.enpassant;
    state.halfmove_clk = parser.halfmove_clk;
    fullmove_clk       = parser.fullmove_clk;

    state.zkey = calculate_key();
    update_check_data();
}

std::string Board::toFEN() const {
    std::ostringstream oss;
    int                empty = 0;

    auto reset_empty = [&]() {
        if (empty > 0) {
            oss << empty;
            empty = 0;
        }
    };

    for (Rank rank = RANK8; rank >= RANK1; --rank) {
        for (File file = FILE1; file <= FILE8; ++file) {
            Piece p = piece_on(file, rank);
            if (p != NO_PIECE) {
                reset_empty();
                oss << p;
            } else {
                ++empty;
            }
        }

        reset_empty();
        if (rank != RANK1)
            oss << '/';
    }

    oss << (turn == WHITE ? " w " : " b ");

    if (can_castle(WHITE) || can_castle(BLACK)) {
        oss << (can_castle_kingside(WHITE) ? "K" : "");
        oss << (can_castle_queenside(WHITE) ? "Q" : "");
        oss << (can_castle_kingside(BLACK) ? "k" : "");
        oss << (can_castle_queenside(BLACK) ? "q" : "");
    } else {
        oss << "-";
    }

    Square ep_sq = enpassant_sq();
    oss << " " << (ep_sq != INVALID ? to_string(ep_sq) : "-");
    oss << " " << +halfmove() << " " << +fullmove();

    return oss.str();
}
