#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "constants.hpp"
#include "types.hpp"

struct PieceSquare {
    Color color;
    PieceType type;
    Square square;
};

// https://www.chessprogramming.org/Forsyth-Edwards_Notation
class FenParser {
   public:
    explicit FenParser(const std::string& fen);

    std::vector<PieceSquare> pieces;
    Color turn          = WHITE;
    CastleRights castle = NO_CASTLE;
    Square enPassantSq  = INVALID;
    U8 hmClock          = 0;
    U32 moveCounter     = 0;

   private:
    void parsePieces(const std::string& section);
    void parseTurn(const std::string& section);
    void parseCastlingRights(const std::string& section);
    void parseEnPassantSq(const std::string& section);
    void parseHalfmove(const std::string& section);
    void parseFullmove(const std::string& section, Color turn);
};
