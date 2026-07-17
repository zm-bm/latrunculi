#include "board/board_notation.hpp"

#include "board/board.hpp"
#include "core/notation.hpp"
#include "movegen/movegen.hpp"

namespace {

char san_piece(PieceType piece) {
    return to_char(make_piece(WHITE, piece));
}

void append_disambiguation(std::string& result, const Board& board, Move move, PieceType piece) {
    bool ambiguous = false;
    bool same_file = false;
    bool same_rank = false;

    for (const Move candidate : movegen::generate_pseudo_legal(board)) {
        if (candidate.from() == move.from() || candidate.to() != move.to() ||
            board.piecetype_on(candidate.from()) != piece ||
            !board.is_legal_generated_move(candidate)) {
            continue;
        }

        ambiguous  = true;
        same_file |= square::file_of(candidate.from()) == square::file_of(move.from());
        same_rank |= square::rank_of(candidate.from()) == square::rank_of(move.from());
    }

    if (!ambiguous)
        return;
    if (!same_file) {
        result += to_char(square::file_of(move.from()));
    } else if (!same_rank) {
        result += to_char(square::rank_of(move.from()));
    } else {
        result += to_char(square::file_of(move.from()));
        result += to_char(square::rank_of(move.from()));
    }
}

} // namespace

std::string to_san(const Board& board, Move move) {
    const Square    from  = move.from();
    const Square    to    = move.to();
    const PieceType piece = board.piecetype_on(from);
    std::string     result;

    if (move.type() == MOVE_CASTLE) {
        result = from < to ? "O-O" : "O-O-O";
    } else {
        if (piece != PAWN) {
            result += san_piece(piece);
            append_disambiguation(result, board, move, piece);
        }

        if (board.is_capture(move)) {
            if (piece == PAWN)
                result += to_char(square::file_of(from));
            result += 'x';
        }

        result += to_string(to);
        if (move.type() == MOVE_PROM) {
            result += '=';
            result += san_piece(move.prom_piece());
        }
    }

    if (board.is_checking_move(move))
        result += '+';
    return result;
}
