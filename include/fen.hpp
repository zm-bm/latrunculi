#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include "types.hpp"
#include "defs.hpp"

// https://www.chessprogramming.org/Forsyth-Edwards_Notation
class FenParser {
   public:
    explicit FenParser(const std::string& fen);
    void parse();

    std::vector<PieceSquare> getPiecePlacement() const;
    Color getActiveColor() const;
    CastleRights getCastlingRights() const;
    Square getEnPassantTarget() const;
    U8 getHalfmoveClock() const;
    U32 getFullmoveNumber() const;

   private:
    std::string fen;
    std::vector<PieceSquare> piece_placement_;
    Color active_color_ = WHITE;
    CastleRights castling_rights_ = NO_CASTLE;
    Square en_passant_target_ = INVALID;
    U8 halfmove_clock_ = 0;
    U32 fullmove_number_ = 0;

    void parsePiecePlacement(const std::string& section);
    void parseActiveColor(const std::string& section);
    void parseCastlingRights(const std::string& section);
    void parseEnPassantTarget(const std::string& section);
    void parseHalfmove(const std::string& section);
    void parseFullmove(const std::string& section, Color turn);
};
