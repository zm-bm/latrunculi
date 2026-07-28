#pragma once

#include <format>
#include <string>

class Board;
struct Move;

// Precondition: move is legal in board's current position.
[[nodiscard]] std::string to_san(const Board& board, Move move);

template <>
struct std::formatter<Board> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    std::format_context::iterator format(const Board& board, std::format_context& ctx) const;
};
