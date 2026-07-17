#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "board/castling.hpp"
#include "core/piece.hpp"
#include "core/square.hpp"
#include "core/types.hpp"

struct PieceSquare {
    Color     color;
    PieceType type;
    Square    square;
};

struct ParsedFen {
    std::vector<PieceSquare> pieces;
    Color                    turn         = WHITE;
    CastleRights             castle       = NO_CASTLE;
    Square                   enpassant    = INVALID;
    std::uint8_t             halfmove_clk = 0;
    int                      fullmove_ply = 0;
};

// Parses four- or six-field Forsyth-Edwards Notation
ParsedFen parse_fen(std::string_view fen);
