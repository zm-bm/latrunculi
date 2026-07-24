#include "board/board.hpp"

#include "board/fen_parser.hpp"
#include "core/notation.hpp"

#include <sstream>

void Board::load_fen(std::string_view fen) {
    const ParsedFen parsed = parse_fen(fen);
    reset();

    for (const auto p : parsed.pieces) {
        add_piece<true>(p.square, p.color, p.type);
        if (p.type == KING)
            king_square[p.color] = p.square;
    }

    auto& state            = this->active_state();
    turn                   = parsed.turn;
    state.castling_rights  = parsed.castling_rights;
    state.enpassant_target = parsed.enpassant_target;
    state.halfmove_clock   = parsed.halfmove_clock;
    absolute_ply           = parsed.absolute_ply;

    refresh_tactical_cache();
    update_legal_enpassant_target();
    state.zkey = recompute_key();
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

    if (has_castling_rights(WHITE) || has_castling_rights(BLACK)) {
        oss << (has_castling_right(CASTLE_KINGSIDE, WHITE) ? "K" : "");
        oss << (has_castling_right(CASTLE_QUEENSIDE, WHITE) ? "Q" : "");
        oss << (has_castling_right(CASTLE_KINGSIDE, BLACK) ? "k" : "");
        oss << (has_castling_right(CASTLE_QUEENSIDE, BLACK) ? "q" : "");
    } else {
        oss << "-";
    }

    const Square ep_sq = enpassant_target();
    oss << " " << (ep_sq != INVALID ? to_string(ep_sq) : "-");
    oss << " " << +halfmove_clock() << " " << +fullmove_number();

    return oss.str();
}
