#include "board/board.hpp"

#include "board/fen_parser.hpp"
#include "core/notation.hpp"

#include <sstream>

// Replaces the current position and clears move and repetition history.
// If parsing fails, the board is unchanged.
void Board::load_fen(std::string_view fen) {
    const ParsedFen parsed = parse_fen(fen);
    clear_position();

    for (const auto& piece : parsed.pieces) {
        add_piece<false>(piece.square, piece.color, piece.type);
        if (piece.type == KING)
            king_square[piece.color] = piece.square;
    }

    auto& state            = ply_state();
    turn                   = parsed.turn;
    state.castling_rights  = parsed.castling_rights;
    state.enpassant_target = parsed.enpassant_target;
    state.halfmove_clock   = parsed.halfmove_clock;
    absolute_ply           = parsed.absolute_ply;

    refresh_tactical_cache();
    refresh_legal_enpassant_target();
    state.zkey = recompute_key();
}

std::string Board::to_fen() const {
    std::ostringstream output;
    int                empty_squares = 0;

    auto flush_empty_squares = [&]() {
        if (empty_squares > 0) {
            output << empty_squares;
            empty_squares = 0;
        }
    };

    for (Rank rank = RANK8; rank >= RANK1; --rank) {
        for (File file = FILE1; file <= FILE8; ++file) {
            const Piece piece = piece_on(file, rank);
            if (piece != NO_PIECE) {
                flush_empty_squares();
                output << piece;
            } else {
                ++empty_squares;
            }
        }

        flush_empty_squares();
        if (rank != RANK1)
            output << '/';
    }

    output << (turn == WHITE ? " w " : " b ");

    if (has_castling_rights(WHITE) || has_castling_rights(BLACK)) {
        if (has_castling_right(CASTLE_KINGSIDE, WHITE))
            output << 'K';
        if (has_castling_right(CASTLE_QUEENSIDE, WHITE))
            output << 'Q';
        if (has_castling_right(CASTLE_KINGSIDE, BLACK))
            output << 'k';
        if (has_castling_right(CASTLE_QUEENSIDE, BLACK))
            output << 'q';
    } else {
        output << '-';
    }

    const Square enpassant_target = this->enpassant_target();
    output << ' ' << (enpassant_target != INVALID ? to_string(enpassant_target) : "-");
    output << ' ' << +halfmove_clock() << ' ' << fullmove_number();

    return output.str();
}
