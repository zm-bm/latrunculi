#include "board/notation.hpp"

#include "board/board.hpp"
#include "core/bitboard.hpp"
#include "core/move.hpp"
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

    const Square from = move.from();
    const Square to   = move.to();

    Bitboard candidates = board.attacks_to(to, board.side_to_move());
    bb::remove(candidates, from);

    while (candidates) {
        const Square candidate_from = bb::lsb_pop(candidates);
        if (board.piece_type_on(candidate_from) != piece_type
            || !board.is_legal_move(Move(candidate_from, to))) {
            continue;
        }

        ambiguous = true;
        same_file |= square::file_of(candidate_from) == square::file_of(from);
        same_rank |= square::rank_of(candidate_from) == square::rank_of(from);
    }

    if (!ambiguous)
        return;
    if (!same_file) {
        san += to_char(square::file_of(from));
    } else if (!same_rank) {
        san += to_char(square::rank_of(from));
    } else {
        san += to_char(square::file_of(from));
        san += to_char(square::rank_of(from));
    }
}

bool has_legal_reply(const Board& board) {
    for (const Move reply : movegen::generate_pseudo_legal(board)) {
        if (board.is_legal_pseudo_move(reply))
            return true;
    }
    return false;
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

    if (board.gives_check(move)) {
        Board next(board);
        next.make(move);
        san += has_legal_reply(next) ? '+' : '#';
    }
    return san;
}

std::format_context::iterator std::formatter<Board>::format(const Board&         board,
                                                            std::format_context& ctx) const {
    auto out = ctx.out();

    for (Rank rank = RANK8; rank >= RANK1; --rank) {
        out = std::format_to(out, "   +---+---+---+---+---+---+---+---+\n");
        out = std::format_to(out, "   |");
        for (File file = FILE1; file <= FILE8; ++file)
            out = std::format_to(out, " {} |", to_char(board.piece_on(file, rank)));
        out = std::format_to(out, " {}\n", to_char(rank));
    }
    out = std::format_to(out, "   +---+---+---+---+---+---+---+---+\n");
    out = std::format_to(out, "     a   b   c   d   e   f   g   h\n\n");
    out = std::format_to(out, "FEN: {}\n", board.to_fen());
    return out;
}
