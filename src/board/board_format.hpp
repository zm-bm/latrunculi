#pragma once

#include <format>
#include <string_view>

#include "board/board.hpp"
#include "core/notation.hpp"

template <>
struct std::formatter<Board> : std::formatter<std::string_view> {
    auto format(const Board& board, std::format_context& ctx) const {
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
        out = std::format_to(out, "FEN: {}\n", board.toFEN());
        return out;
    }
};
