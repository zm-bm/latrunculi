#pragma once

#include <string>
#include <vector>

#include "defs.hpp"

struct PieceSquare {
    Color     color;
    PieceType type;
    Square    square;
};

// https://www.chessprogramming.org/Forsyth-Edwards_Notation
class FenParser {
public:
    explicit FenParser(const std::string& fen);

    std::vector<PieceSquare> pieces;

    Color        turn         = WHITE;
    CastleRights castle       = NO_CASTLE;
    Square       enpassant    = INVALID;
    uint8_t      halfmove_clk = 0;
    uint32_t     fullmove_clk = 0;

private:
    void parse_pieces(const std::string& section);
    void parse_turn(const std::string& section);
    void parse_castles(const std::string& section);
    void parse_enpassant(const std::string& section);
    void parse_halfmove(const std::string& section);
    void parse_fullmove(const std::string& section, Color turn);
};
