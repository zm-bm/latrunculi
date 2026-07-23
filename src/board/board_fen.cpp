#include "board/board.hpp"

#include "board/fen.hpp"
#include "core/notation.hpp"

#include <sstream>

void Board::load_fen(const std::string& fen) {
    const ParsedFen parsed = parse_fen(fen);
    reset();

    for (const auto p : parsed.pieces) {
        add_piece<true>(p.square, p.color, p.type);
        if (p.type == KING)
            king_square[p.color] = p.square;
    }

    auto& state            = this->active_state();
    turn                   = parsed.turn;
    state.castle           = parsed.castle;
    state.enpassant_target = parsed.enpassant_target;
    state.halfmove_clk     = parsed.halfmove_clk;
    absolute_ply           = parsed.absolute_ply;

    update_check_data();
    update_legal_enpassant_target();
    state.zkey = calculate_key();
}

std::string Board::to_fen() const {
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

    const Square ep_sq = enpassant_target();
    oss << " " << (ep_sq != INVALID ? to_string(ep_sq) : "-");
    oss << " " << +halfmove() << " " << +fullmove();

    return oss.str();
}
