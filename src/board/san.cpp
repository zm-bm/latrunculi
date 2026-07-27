#include "board/san.hpp"

#include "board/board.hpp"
#include "core/move_geometry.hpp"
#include "core/notation.hpp"
#include "movegen/movegen.hpp"

namespace {

char san_piece(PieceType piece_type) {
    return to_char(make_piece(WHITE, piece_type));
}

void append_disambiguation(std::string& san, const Board& board, Move move, PieceType piece_type) {
    bool ambiguous = false;
    bool same_file = false;
    bool same_rank = false;

    for (const Move candidate : movegen::generate_pseudo_legal(board)) {
        if (candidate.from() == move.from() || candidate.to() != move.to()
            || board.piece_type_on(candidate.from()) != piece_type
            || !board.is_legal_generated_move(candidate)) {
            continue;
        }

        ambiguous = true;
        same_file |= square::file_of(candidate.from()) == square::file_of(move.from());
        same_rank |= square::rank_of(candidate.from()) == square::rank_of(move.from());
    }

    if (!ambiguous)
        return;
    if (!same_file) {
        san += to_char(square::file_of(move.from()));
    } else if (!same_rank) {
        san += to_char(square::rank_of(move.from()));
    } else {
        san += to_char(square::file_of(move.from()));
        san += to_char(square::rank_of(move.from()));
    }
}

} // namespace

std::string to_san(const Board& board, Move move) {
    const Square    from       = move.from();
    const Square    to         = move.to();
    const PieceType piece_type = board.piece_type_on(from);
    std::string     san;

    if (move.type() == MOVE_CASTLE) {
        san = move_geometry::castle_side(from, to) == CASTLE_KINGSIDE ? "O-O" : "O-O-O";
    } else {
        if (piece_type != PAWN) {
            san += san_piece(piece_type);
            append_disambiguation(san, board, move, piece_type);
        }

        if (board.is_capture(move)) {
            if (piece_type == PAWN)
                san += to_char(square::file_of(from));
            san += 'x';
        }

        san += to_string(to);
        if (move.type() == MOVE_PROM) {
            san += '=';
            san += san_piece(move.prom_piece());
        }
    }

    if (board.gives_check(move))
        san += '+';
    return san;
}
